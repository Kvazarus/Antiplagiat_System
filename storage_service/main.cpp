#define _CRT_SECURE_NO_WARNINGS
#include "crow_all.h"
#include <pqxx/pqxx>
#include <cpr/cpr.h>
#include <iostream>
#include <fstream>
#include <filesystem>

std::string get_db_connection_string() {
    std::string user = std::getenv("POSTGRES_USER") ? std::getenv("POSTGRES_USER") : "user";
    std::string password = std::getenv("POSTGRES_PASSWORD") ? std::getenv("POSTGRES_PASSWORD") : "password";
    std::string host = std::getenv("POSTGRES_HOST") ? std::getenv("POSTGRES_HOST") : "db";
    std::string dbname = std::getenv("POSTGRES_DB") ? std::getenv("POSTGRES_DB") : "antiplagiat_db";

    return "postgresql://" + user + ":" + password + "@" + host + ":5432/" + dbname;
}

int main() {
    crow::SimpleApp app;

    std::filesystem::create_directories("./uploads");

    CROW_ROUTE(app, "/upload").methods("POST"_method)([](const crow::request& req) {
        crow::multipart::message file_message(req);

        std::string student_name = file_message.get_part_by_name("student").body;
        std::string task_id = file_message.get_part_by_name("task_id").body;
        auto file_part = file_message.get_part_by_name("file");

        if (student_name.empty() || task_id.empty() || file_part.body.empty()) {
            return crow::response(400, "Missing required fields (student, task_id, file)");
        }

        std::string filename = task_id + "_" + student_name + ".txt";
        std::string filepath = "./uploads/" + filename;

        std::ofstream out_file(filepath);
        out_file << file_part.body;
        out_file.close();

        int submission_id = -1;

        try {
            pqxx::connection c(get_db_connection_string());
            pqxx::work w(c);

            std::string sql =
                "INSERT INTO submissions (student_name, task_id, filename) "
                "VALUES (" + w.quote(student_name) + ", " + w.quote(task_id) + ", " + w.quote(filename) + ") "
                                                                                                          "RETURNING id";

            pqxx::result r = w.exec(sql);
            submission_id = r[0][0].as<int>();

            w.commit();
        } catch (const std::exception &e) {
            std::cerr << "DB Error: " << e.what() << std::endl;
            return crow::response(500, "Database error");
        }

        crow::json::wvalue analysis_req;
        analysis_req["submission_id"] = submission_id;

        std::cout << "Sending request to Analysis Service for submission_id: " << submission_id << std::endl;

        cpr::Response r_http = cpr::Post(
            cpr::Url{"http://analysis_service:8080/internal/analyze"},
            cpr::Body{analysis_req.dump()},
            cpr::Header{{"Content-Type", "application/json"}}
        );

        bool is_plagiarism = false;
        std::string operation_status = "checked";

        if (r_http.status_code == 200) {
            try {
                auto json_resp = crow::json::load(r_http.text);
                if (json_resp) {
                    is_plagiarism = json_resp["is_plagiarism"].b();
                } else {
                    std::cerr << "Error parsing Analysis JSON" << std::endl;
                    operation_status = "analysis_format_error";
                }
            } catch(const std::exception& e) {
                std::cerr << "JSON Exception: " << e.what() << std::endl;
                operation_status = "analysis_format_error";
            }
        }
        else {
            std::cerr << "Analysis Service Failed! Code: " << r_http.status_code << std::endl;
            std::cerr << "Response text: " << r_http.text << std::endl;
            operation_status = "uploaded_but_check_failed";
        }

        crow::json::wvalue result;
        result["submission_id"] = submission_id;
        result["filename"] = filename;
        result["status"] = operation_status;
        result["is_plagiarism"] = is_plagiarism;

        return crow::response(201, result);
    });

    app.port(8080).multithreaded().run();
}