#include "WSA.hpp"
#include "Router.hpp"
#include "UserRoutes.hpp"
#include "ThreadPool.hpp"
#include <thread>
#include <string>
#include <iostream>
#include <csignal>

static std::atomic_bool g_running{ true };

void handleShutdown(int) 
{
    g_running = false;

}
int main() {
    std::signal(SIGINT, handleShutdown);
    try {
        RadixTree router;
        addUserRoutes(router);


        ThreadPool threadPool(std::thread::hardware_concurrency());

        std::cout << "Creating server with : " << std::thread::hardware_concurrency() << " threads. \n";

        WSAHandler server("2700", router, threadPool, g_running);
        server.run();   // all logic inside
    }
    catch (const std::exception& e) {
        std::cout << "Fatal error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}