#define _CRT_SECURE_NO_WARNINGS
#include "crow_all.h"
#include <cpr/cpr.h>
#include <iostream>

int main() {
    crow::SimpleApp app;

    CROW_ROUTE(app, "/api/upload").methods("POST"_method)([](const crow::request& req) {
        std::cout << "Gateway: Passthrough upload (" << req.body.size() << " bytes)..." << std::endl;

        std::string content_type = req.get_header_value("Content-Type");

        cpr::Response r = cpr::Post(
            cpr::Url{"http://storage_service:8080/upload"},
            cpr::Body{req.body},
            cpr::Header{{"Content-Type", content_type}}
        );

        std::cout << "Gateway: Storage responded with code " << r.status_code << std::endl;

        crow::response resp;
        resp.code = r.status_code;
        resp.body = r.text;
        resp.add_header("Content-Type", "application/json");
        return resp;
    });

    CROW_ROUTE(app, "/api/reports/<int>").methods("GET"_method)([](int submission_id) {
        try {
            cpr::Response r = cpr::Get(
                cpr::Url{"http://analysis_service:8080/report/" + std::to_string(submission_id)}
            );
            crow::response resp;
            resp.code = r.status_code;
            resp.body = r.text;
            resp.add_header("Content-Type", "application/json");
            return resp;
        } catch (...) {
            return crow::response(500, "Gateway Error");
        }
    });

    CROW_ROUTE(app, "/api/reports/task/<string>").methods("GET"_method)([](std::string task_id) {
        try {
            cpr::Response r = cpr::Get(
                cpr::Url{"http://analysis_service:8080/internal/reports/task/" + task_id}
            );
            crow::response resp;
            resp.code = r.status_code;
            resp.body = r.text;
            resp.add_header("Content-Type", "application/json");
            return resp;
        } catch (...) {
            return crow::response(500, "Gateway Error");
        }
    });

    app.port(8080).multithreaded().run();
}