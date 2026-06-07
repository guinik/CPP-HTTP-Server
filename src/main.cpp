#include "WSA.hpp"
#include "Router.hpp"
#include <string>
#include <iostream>

int main() {
    try {
        RadixTree router;

        router.add("/users", "GET", [](const HTTPRequest& req) -> HTTPResponse{ 
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



        WSAHandler server("2700", router);
        server.run();   // all logic inside
    }
    catch (const std::exception& e) {
        std::cout << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}