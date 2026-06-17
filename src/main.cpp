#include "HttpServer.hpp"
#include "Router.hpp"
#include "UserRoutes.hpp"
#include "ThreadPool.hpp"
#include "Logger.hpp"
#include <thread>
#include <string>
#include <format>
#include <csignal>

// atomic_bool is safe in a signal handler only when it is lock-free (no mutex
// internally).  The static_assert turns this assumption into a compile error on
// any platform where it does not hold rather than silent undefined behaviour.
static_assert(std::atomic<bool>::is_always_lock_free,
    "atomic_bool must be lock-free to be used safely in a signal handler");

static std::atomic_bool g_running{ true };

void handleShutdown(int)
{
    g_running.store(false, std::memory_order_relaxed);
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, handleShutdown);
    try {
        std::string port = "2700";
        if (argc > 1) {
            try {
                int portNum = std::stoi(argv[1]);
                if (portNum < 1 || portNum > 65535)
                    throw std::out_of_range("must be 1-65535");
                port = argv[1];
            } catch (const std::exception& e) {
                Log::error(std::format("Invalid port '{}' ({}); using default {}.",
                           argv[1], e.what(), port));
            }
        }

        RouteTrie router;
        addUserRoutes(router);

        ThreadPool threadPool(std::thread::hardware_concurrency());
        Log::info(std::format("Creating server with: {} threads.", std::thread::hardware_concurrency()));

        HttpServer server(port, router, threadPool, g_running);
        server.run();
    }
    catch (const std::exception& e) {
        Log::error(std::format("Fatal error: {}", e.what()));
        return 1;
    }

    return 0;
}
