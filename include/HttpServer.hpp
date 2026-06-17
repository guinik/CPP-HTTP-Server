#pragma once
#include <stdexcept>
#include <format>
#include <atomic>
#include <chrono>
#include "SocketGuard.hpp"
#include "AddrInfo.hpp"
#include "HandleConnection.hpp"
#include "IRouter.hpp"
#include "ThreadPool.hpp"

#ifdef _WIN32
class WinSocketGuard {
public:
    WinSocketGuard() {
        if (WSAStartup(MAKEWORD(2, 2), &_wsaData) != 0)
            throw std::runtime_error("WSA Startup Failed");
    }
    ~WinSocketGuard() { WSACleanup(); }
    WinSocketGuard(const WinSocketGuard&) = delete;
    WinSocketGuard& operator=(const WinSocketGuard&) = delete;
private:
    WSADATA _wsaData;
};
#endif

class HttpServer {
public:
    static constexpr size_t kMaxConnections = 1000;
    static constexpr auto   kDefaultShutdownTimeout = std::chrono::seconds(5);

    HttpServer(const std::string& PORT, IRouter& router, ThreadPool& threadPool,
               std::atomic_bool& running,
               std::chrono::seconds shutdownTimeout = kDefaultShutdownTimeout)
        : _PORT(PORT), _router(router), _threadPool(threadPool),
          _running(running), _shutdownTimeout(shutdownTimeout) {}

    void run();

private:
#ifdef _WIN32
    WinSocketGuard _winSocketInitializer;
#endif
    std::string          _PORT;
    IRouter&             _router;
    ThreadPool&          _threadPool;
    std::atomic_bool&    _running;
    std::chrono::seconds _shutdownTimeout;
    std::atomic<size_t>  _activeConnections{ 0 };
};
