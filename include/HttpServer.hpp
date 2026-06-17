#pragma once
#include <stdexcept>
#include <format>
#include <atomic>
#include <chrono>
#include <mutex>
#include <condition_variable>
#include "SocketGuard.hpp"
#include "AddrInfo.hpp"
#include "HandleConnection.hpp"
#include "IRouter.hpp"
#include "Metrics.hpp"
#include "ThreadPool.hpp"
#include "ServerConfig.hpp"

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
    HttpServer(const IRouter& router, ThreadPool& threadPool,
               std::atomic_bool& running, Metrics& metrics,
               const ServerConfig& config = ServerConfig{})
        : _config(config), _router(router),
          _threadPool(threadPool), _running(running), _metrics(metrics) {}

    void run();

private:
#ifdef _WIN32
    WinSocketGuard _winSocketInitializer;
#endif
    ServerConfig         _config;
    const IRouter&       _router;
    ThreadPool&          _threadPool;
    std::atomic_bool&    _running;
    Metrics&             _metrics;

    std::mutex              _drainMtx;
    std::condition_variable _drainCv;
};
