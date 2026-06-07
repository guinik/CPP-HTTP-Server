#include "WSA.hpp"
#include "Router.hpp"
#include "UserRoutes.hpp"
#include <string>
#include <iostream>

int main() {
    try {
        RadixTree router;

        addUserRoutes(router);

        WSAHandler server("2700", router);
        server.run();   // all logic inside
    }
    catch (const std::exception& e) {
        std::cout << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}