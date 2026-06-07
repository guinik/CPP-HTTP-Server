#include "UserRoutes.hpp"


void addUserRoutes(RadixTree& router)
{
    router.add("/users", "GET", [](const HTTPRequest& req) -> HTTPResponse {
        HTTPResponse response;
        response.code = "200";
        response.body = "hellooooo";
        response.version = "HTTP/1.1";
        response.reason = "All good";
        return response;
        });

    router.add("/users/:id", "GET", [](const HTTPRequest& req) -> HTTPResponse {
        HTTPResponse response;
        response.version = "HTTP/1.1";
        response.code = "200";
        response.reason = "OK";
        response.body = "user id is: " + req.params.at("id");
        return response;
        });



};