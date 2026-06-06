#include "WSA.hpp"
#include <iostream>

int main() {
    try {
        WSAHandler server("2700");
        server.run();   // all logic inside
    }
    catch (const std::exception& e) {
        std::cout << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}