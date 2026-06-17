#pragma once
#include "Router.hpp"

struct CorsOptions{
    std::vector<std::string> allowedOrigins;
    std::vector<std::string> allowedMethods;
    std::vector<std::string> allowedHeaders;
};

inline MiddleWare makeCors(CorsOptions corsOpt)
{
    return [corsOpt](HTTPRequest& req, HTTPResponse& res, Next next) {

        // No Origin header means this is not a browser cross-origin request
        // (curl, server-to-server, direct navigation). CORS rules don't apply.
        if (!req.head.headers.count("Origin")) {
            next();
            return;
        }

        std::string origin = req.head.headers["Origin"];

        bool allowed = false;
        for (auto& allowedOr : corsOpt.allowedOrigins) {
            if (allowedOr == "*")
            {
                allowed = true;
                origin = "*";
                break;
            }
            if (allowedOr == origin) {
                allowed = true;
                break;
            }
        }
        if (!allowed) {
            res.code = "403";
            res.reason = "Unauthorized origin";
            return;
        }

        std::string methods;
        for (size_t i = 0; i < corsOpt.allowedMethods.size(); i++) {
            methods += corsOpt.allowedMethods[i];
            if (i < corsOpt.allowedMethods.size() - 1)
                methods += ", ";
        }

        std::string headers;
        for (size_t i = 0; i < corsOpt.allowedHeaders.size(); i++) {
            headers += corsOpt.allowedHeaders[i];
            if (i < corsOpt.allowedHeaders.size() - 1)
                headers += ", ";
        }

        res.headers["Access-Control-Allow-Origin"] = origin;
        res.headers["Access-Control-Allow-Methods"] = methods;
        res.headers["Access-Control-Allow-Headers"] = headers;

        if (req.head.method == "OPTIONS") {
            res.code = "204";
            res.reason = "No Content";
            return;
        }

        next();
        };
}

