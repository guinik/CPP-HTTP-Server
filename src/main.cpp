#include "HttpServer.hpp"
#include "Router.hpp"
#include "UserRoutes.hpp"
#include "ThreadPool.hpp"
#include "Logger.hpp"
#include <thread>
#include <string>
#include <format>
#include <csignal>

static std::atomic_bool g_running{ true };

void handleShutdown(int) 
{
    g_running = false;

}
int main(int argc, char* argv[]) {
    std::signal(SIGINT, handleShutdown);
    try {
        std::string port = "2700";
        if (argc > 1)
        {
            port = argv[1];
        }

        RouteTrie router;
        addUserRoutes(router);


        ThreadPool threadPool(std::thread::hardware_concurrency());

        Log::info(std::format("Creating server with: {} threads.", std::thread::hardware_concurrency()));

        HttpServer server(port, router, threadPool, g_running);
        server.run();   // all logic inside
    }
    catch (const std::exception& e) {
        Log::error(std::format("Fatal error: {}", e.what()));
        return 1;
    }

    return 0;
}