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

// Resolved once at startup so every request sees the same root regardless of
// any later chdir(), and so canonical() failures surface immediately rather
// than silently 404-ing every request.
static const std::filesystem::path kPublicRoot = []() -> std::filesystem::path {
    try {
        return std::filesystem::canonical(std::filesystem::current_path() / "public");
    } catch (const std::filesystem::filesystem_error&) {
        return {};
    }
}();

// Files larger than this are refused to prevent reading a multi-GB file into
// a std::string and OOM-ing the process.
static constexpr uintmax_t kMaxServedFileBytes = 10ULL * 1024 * 1024;

void addUserRoutes(RouteTrie& router)
{
    router.add("/users", "GET", { requestLogger, parseJson, userCorsMiddleWare },
        [](const HTTPRequest& req, HTTPResponse& response) -> void {
            response.code = "200";
            response.reason = "All good";
            response.headers["Content-Type"] = "application/json";
            std::string page  = req.head.queryParams.count("page")  ? req.head.queryParams.at("page")  : "not set";
            std::string limit = req.head.queryParams.count("limit") ? req.head.queryParams.at("limit") : "not set";

            nlohmann::json responseJson = {
                {"message", req.body.json.contains("message") ? req.body.json["message"] : "No message"},
                {"page",  page},
                {"limit", limit}
            };
            response.body = responseJson.dump();
        });

    router.add("/users/:id", "GET", { requestLogger, userCorsMiddleWare },
        [](const HTTPRequest& req, HTTPResponse& response) -> void {
            response.code   = "200";
            response.reason = "OK";
            response.body   = "user id issss: " + req.head.params.at("id");
        });

    router.add("/public/*", "GET", { requestLogger, userCorsMiddleWare },
        [](const HTTPRequest& req, HTTPResponse& response) -> void {
            namespace fs = std::filesystem;

            if (kPublicRoot.empty()) {
                response.code   = "503";
                response.reason = "Service Unavailable";
                response.body   = R"({"error":"Static file root not available"})";
                response.headers["Content-Type"] = "application/json";
                return;
            }

            fs::path requestedPath;
            try {
                requestedPath = fs::canonical(kPublicRoot / req.head.params.at("*"));
            } catch (const fs::filesystem_error&) {
                response.code   = "404";
                response.reason = "Not Found";
                response.body   = "File not found";
                return;
            }

            auto [rootEnd, _] = std::mismatch(
                kPublicRoot.begin(), kPublicRoot.end(), requestedPath.begin());
            if (rootEnd != kPublicRoot.end()) {
                response.code   = "403";
                response.reason = "Forbidden";
                return;
            }

            // Refuse files that would OOM the process if slurped into a string.
            std::error_code ec;
            auto fileSize = fs::file_size(requestedPath, ec);
            if (ec || fileSize > kMaxServedFileBytes) {
                response.code   = "403";
                response.reason = "Forbidden";
                return;
            }

            std::ifstream file(requestedPath, std::ios::binary);
            if (!file.is_open()) {
                response.code   = "404";
                response.reason = "Not Found";
                response.body   = "File not found";
                return;
            }

            std::string content((std::istreambuf_iterator<char>(file)),
                                 std::istreambuf_iterator<char>());

            std::string pathStr     = requestedPath.string();
            std::string contentType = "text/plain";
            if      (pathStr.ends_with(".html")) contentType = "text/html";
            else if (pathStr.ends_with(".css"))  contentType = "text/css";
            else if (pathStr.ends_with(".js"))   contentType = "application/javascript";
            else if (pathStr.ends_with(".json")) contentType = "application/json";

            response.code   = "200";
            response.reason = "OK";
            response.headers["Content-Type"] = contentType;
            response.body   = content;
        });

    router.add("/users", "OPTIONS", { requestLogger, userCorsMiddleWare },
        [](const HTTPRequest&, HTTPResponse& response) -> void {
            response.code   = "204";
            response.reason = "No Content";
        });
}
