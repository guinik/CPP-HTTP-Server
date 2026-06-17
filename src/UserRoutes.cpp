#include "UserRoutes.hpp"
#include "Utils.hpp"
#include "Cors.hpp"
#include <iostream>
#include <fstream>
#include <filesystem>



CorsOptions userCors = {
    .allowedOrigins = {"*"},
    .allowedMethods = {"GET", "POST", "PUT", "DELETE", "OPTIONS"},
    .allowedHeaders = {"Content-Type", "Authorization"}
};

MiddleWare userCorsMiddleWare = makeCors(userCors);

void addUserRoutes(RadixTree& router)
{
    router.add("/users", "GET", { requestLogger, parseJson, userCorsMiddleWare }, [](const HTTPRequest& req, HTTPResponse& response) -> void {
        response.code = "200";
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



    router.add("/users/:id", "GET", { requestLogger, userCorsMiddleWare }, [](const HTTPRequest& req, HTTPResponse& response) -> void {
        response.code = "200";
        response.reason = "OK";
        response.body = "user id issss: " + req.head.params.at("id");
        });

    router.add("/public/*", "GET", { requestLogger, userCorsMiddleWare }, [](const HTTPRequest& req, HTTPResponse& response) -> void {
            namespace fs = std::filesystem;

            const fs::path publicRoot = fs::canonical(fs::current_path() / "public");

            // canonical() throws if the path does not exist, eliminating the
            // TOCTOU window that weakly_canonical() leaves for non-existent
            // paths (lexical normalisation without a disk check).
            fs::path requestedPath;
            try {
                requestedPath = fs::canonical(publicRoot / req.head.params.at("*"));
            } catch (const fs::filesystem_error&) {
                response.code = "404";
                response.reason = "Not Found";
                response.body = "File not found";
                return;
            }

            auto [rootEnd, _] = std::mismatch(publicRoot.begin(), publicRoot.end(), requestedPath.begin());
            if (rootEnd != publicRoot.end()) {
                response.code = "403";
                response.reason = "Forbidden";
                return;
            }

            std::ifstream file(requestedPath);

            if (!file.is_open()) {
                response.code = "404";
                response.reason = "Not Found";
                response.body = "File not found";
                return;
            }

            std::string content((std::istreambuf_iterator<char>(file)),
                std::istreambuf_iterator<char>());

            std::string pathStr = requestedPath.string();
            std::string contentType = "text/plain";
            if (pathStr.ends_with(".html"))      contentType = "text/html";
            else if (pathStr.ends_with(".css"))  contentType = "text/css";
            else if (pathStr.ends_with(".js"))   contentType = "application/javascript";
            else if (pathStr.ends_with(".json")) contentType = "application/json";

            response.code = "200";
            response.reason = "OK";
            response.headers["Content-Type"] = contentType;
            response.body = content;
        });

    router.add("/users", "OPTIONS", { requestLogger, userCorsMiddleWare}, [](const HTTPRequest& req, HTTPResponse& response) -> void {
        response.code = "204";
        response.reason = "No Content";
        });
};