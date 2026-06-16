#include "UserRoutes.hpp"
#include "Utils.hpp"
#include "Cors.hpp"
#include <iostream>
#include <fstream>



CorsOptions userCors = {
    .allowedOrigins = {"*"},
    .allowedMethods = {"GET", "POST", "PUT", "DELETE", "OPTIONS"},
    .allowedHeaders = {"Content-Type", "Authorization"}
};

MiddleWare userCorsMiddleWare = makeCors(userCors);

void addUserRoutes(RadixTree& router)
{
    router.add("/users", "GET", {parseJson, userCorsMiddleWare }, [](const HTTPRequest& req, HTTPResponse& response) -> void {
        response.code = "200";
        response.version = "HTTP/1.1";
        response.reason = "All good";
        response.headers["Content-Type"] = "application/json";
        std::string page = req.head.queryParams.count("page") ? req.head.queryParams.at("page") : "not set";
        std::string limit = req.head.queryParams.count("limit") ? req.head.queryParams.at("limit") : "not set";

        nlohmann::json responseJson = {
            {"message", req.body.json.contains("message") ? req.body.json["message"] : "No message"},
            {"page", page},
            {"limit", limit}
        };
        response.body = responseJson.dump();

        });



    router.add("/users/:id", "GET", { userCorsMiddleWare }, [](const HTTPRequest& req, HTTPResponse& response) -> void {
        response.version = "HTTP/1.1";
        response.code = "200";
        response.reason = "OK";
        response.body = "user id issss: " + req.head.params.at("id");
        });

    router.add("/public/*", "GET", { userCorsMiddleWare }, [](const HTTPRequest& req, HTTPResponse& response) -> void {
            std::string filePath = req.head.params.at("*");

            // open the file
            std::ifstream file(filePath);

            if (!file.is_open()) {
                response.version = "HTTP/1.1";
                response.code = "404";
                response.reason = "Not Found";
                response.body = "File not found: " + filePath;
                return;
            }

            // read the file contents
            std::string content((std::istreambuf_iterator<char>(file)),
                std::istreambuf_iterator<char>());

            // detect content type
            std::string contentType = "text/plain";
            if (filePath.ends_with(".html")) contentType = "text/html";
            else if (filePath.ends_with(".css"))  contentType = "text/css";
            else if (filePath.ends_with(".js"))   contentType = "application/javascript";
            else if (filePath.ends_with(".json")) contentType = "application/json";

            response.version = "HTTP/1.1";
            response.code = "200";
            response.reason = "OK";
            response.headers["Content-Type"] = contentType;
            response.body = content;
        });

    router.add("/users", "OPTIONS", {userCorsMiddleWare}, [](const HTTPRequest& req, HTTPResponse& response) -> void {
        response.code = "204";
        response.reason = "No Content";
        response.version = "HTTP/1.1";
        });
};