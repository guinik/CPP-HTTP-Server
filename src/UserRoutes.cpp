#include "UserRoutes.hpp"
#include "Utils.hpp"

void addUserRoutes(RadixTree& router)
{
    router.add("/users", "GET", { parseJson }, [](const HTTPRequest& req, HTTPResponse& response) -> void {
        response.code = "200";
        response.body = req.body.json.dump();
        response.version = "HTTP/1.1";
        response.reason = "All good";
        response.headers["Content-Type"] = "application/json";
        response.headers["Content-Length"] = std::to_string(response.body.size());
        });



    router.add("/users/:id", "GET", {}, [](const HTTPRequest& req, HTTPResponse& response) -> void {
        response.version = "HTTP/1.1";
        response.code = "200";
        response.reason = "OK";
        response.body = "user id is: " + req.head.params.at("id");
        });

};