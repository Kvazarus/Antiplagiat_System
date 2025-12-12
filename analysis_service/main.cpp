#define _CRT_SECURE_NO_WARNINGS
#include "crow_all.h"
#include <pqxx/pqxx>
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <iomanip>

std::string url_encode(const std::string &value) {
    std::ostringstream escaped;
    escaped.fill('0');
    escaped << std::hex;

    for (std::string::const_iterator i = value.begin(), n = value.end(); i != n; ++i) {
        std::string::value_type c = (*i);

        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
            escaped << c;
            continue;
        }

        escaped << std::uppercase;
        escaped << '%' << std::setw(2) << int((unsigned char) c);
        escaped << std::nouppercase;
    }

    return escaped.str();
}

std::string read_file(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) return "";
    std::stringstream buffer;
    buffer << f.rdbuf();
    return buffer.str();
}

std::string get_db_connection_string() {
    std::string user = std::getenv("POSTGRES_USER") ? std::getenv("POSTGRES_USER") : "user";
    std::string password = std::getenv("POSTGRES_PASSWORD") ? std::getenv("POSTGRES_PASSWORD") : "password";
    std::string host = std::getenv("POSTGRES_HOST") ? std::getenv("POSTGRES_HOST") : "db";
    std::string dbname = std::getenv("POSTGRES_DB") ? std::getenv("POSTGRES_DB") : "antiplagiat_db";
    return "postgresql://" + user + ":" + password + "@" + host + ":5432/" + dbname;
}

struct AnalysisResult {
    bool is_plagiarism;
    int similar_to_id;
    bool success;
};

AnalysisResult perform_analysis_logic(int submission_id, pqxx::work& w, bool force_update = false) {
    try {
        if (!force_update) {
            pqxx::result r_exist = w.exec("SELECT is_plagiarism, similar_to_id FROM reports WHERE submission_id = " + w.quote(submission_id));
            if (!r_exist.empty()) {
                int sim_id = r_exist[0]["similar_to_id"].is_null() ? 0 : r_exist[0]["similar_to_id"].as<int>();
                return { r_exist[0]["is_plagiarism"].as<bool>(), sim_id, true };
            }
        } else {
            w.exec0("DELETE FROM reports WHERE submission_id = " + w.quote(submission_id));
        }

        pqxx::result r_curr = w.exec("SELECT filename, task_id, student_name FROM submissions WHERE id = " + w.quote(submission_id));
        if (r_curr.empty()) return { false, 0, false };

        std::string current_filename = r_curr[0]["filename"].as<std::string>();
        std::string task_id = r_curr[0]["task_id"].as<std::string>();
        std::string current_student = r_curr[0]["student_name"].as<std::string>();

        std::string current_text = read_file("./uploads/" + current_filename);
        if (current_text.empty()) return { false, 0, false };

        std::string sql_prev =
            "SELECT id, filename FROM submissions "
            "WHERE task_id = " + w.quote(task_id) +
                " AND id < " + w.quote(submission_id) +
                " AND student_name != " + w.quote(current_student);

        pqxx::result r_prev = w.exec(sql_prev);

        bool is_plagiarism = false;
        int original_submission_id = 0;

        for (auto row : r_prev) {
            std::string prev_filename = row["filename"].as<std::string>();
            std::string prev_text = read_file("./uploads/" + prev_filename);
            if (!current_text.empty() && current_text == prev_text) {
                is_plagiarism = true;
                original_submission_id = row["id"].as<int>();
                break;
            }
        }

        std::string similar_val = (original_submission_id == 0) ? "NULL" : std::to_string(original_submission_id);
        std::string plagiat_val = is_plagiarism ? "TRUE" : "FALSE";

        w.exec0("INSERT INTO reports (submission_id, is_plagiarism, similar_to_id) VALUES (" +
            w.quote(submission_id) + ", " + plagiat_val + ", " + similar_val + ")");

        return { is_plagiarism, original_submission_id, true };

    } catch (const std::exception& e) {
        std::cerr << "Logic Error: " << e.what() << std::endl;
        return { false, 0, false };
    }
}

int main() {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/internal/analyze").methods("POST"_method)([](const crow::request& req) {
        auto json = crow::json::load(req.body);
        if (!json || !json.has("submission_id")) return crow::response(400);
        int sub_id = json["submission_id"].i();

        try {
            pqxx::connection c(get_db_connection_string());
            pqxx::work w(c);
            auto result = perform_analysis_logic(sub_id, w);
            w.commit();
            crow::json::wvalue res;
            res["submission_id"] = sub_id;
            res["is_plagiarism"] = result.is_plagiarism;
            res["status"] = result.success ? "checked" : "error";
            return crow::response(200, res);
        } catch (...) { return crow::response(500); }
    });

    CROW_ROUTE(app, "/report/<int>").methods("GET"_method)([](int submission_id){
        try {
            pqxx::connection c(get_db_connection_string());
            pqxx::work w(c);

            auto result = perform_analysis_logic(submission_id, w);

            pqxx::result res_info = w.exec("SELECT student_name, task_id, filename FROM submissions WHERE id=" + w.quote(submission_id));

            w.commit();

            if (!result.success || res_info.empty()) {
                return crow::response(404, "Report generation failed");
            }

            crow::json::wvalue json_resp;
            json_resp["submission_id"] = submission_id;
            json_resp["student"] = res_info[0]["student_name"].as<std::string>();
            json_resp["task_id"] = res_info[0]["task_id"].as<std::string>();
            json_resp["is_plagiarism"] = result.is_plagiarism;
            if (result.similar_to_id != 0) json_resp["similar_to_id"] = result.similar_to_id;
            else json_resp["similar_to_id"] = nullptr;

            std::string filename = res_info[0]["filename"].as<std::string>();
            std::string text = read_file("./uploads/" + filename);

            if (!text.empty()) {
                // Обрезаем текст до 1500 символов, чтобы URL не был слишком длинным
                std::string short_text = text.substr(0, 1500);
                std::string cloud_url = "https://quickchart.io/wordcloud?text=" + url_encode(short_text);
                json_resp["wordcloud_url"] = cloud_url;
            } else {
                json_resp["wordcloud_url"] = nullptr;
            }

            return crow::response(200, json_resp);

        } catch (const std::exception &e) {
            return crow::response(500, e.what());
        }
    });

    CROW_ROUTE(app, "/internal/reports/task/<string>").methods("GET"_method)([](std::string task_id){
        try {
            pqxx::connection c(get_db_connection_string());
            pqxx::work w(c);
            std::string sql = "SELECT id FROM submissions WHERE task_id = " + w.quote(task_id) + " ORDER BY upload_timestamp DESC";
            pqxx::result res_ids = w.exec(sql);
            std::vector<crow::json::wvalue> reports_list;
            for (auto row : res_ids) perform_analysis_logic(row["id"].as<int>(), w);

            std::string sql_full = "SELECT s.id, s.student_name, s.upload_timestamp, r.is_plagiarism FROM submissions s LEFT JOIN reports r ON s.id = r.submission_id WHERE s.task_id = " + w.quote(task_id) + " ORDER BY s.upload_timestamp DESC";
            pqxx::result res_final = w.exec(sql_full);

            for (auto row : res_final) {
                crow::json::wvalue item;
                item["submission_id"] = row["id"].as<int>();
                item["student"] = row["student_name"].as<std::string>();
                item["timestamp"] = row["upload_timestamp"].as<std::string>();
                if (!row["is_plagiarism"].is_null()) item["is_plagiarism"] = row["is_plagiarism"].as<bool>();
                else item["is_plagiarism"] = nullptr;
                reports_list.push_back(item);
            }
            w.commit();
            crow::json::wvalue final_json;
            final_json["task_id"] = task_id;
            final_json["reports"] = std::move(reports_list);
            return crow::response(200, final_json);
        } catch (const std::exception &e) { return crow::response(500, "Database error"); }
    });

    app.port(8080).multithreaded().run();
}