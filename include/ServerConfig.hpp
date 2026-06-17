#pragma once
#include <string>
#include <cstddef>
#include <thread>
#include <chrono>
#include "Logger.hpp"

struct ServerConfig {
    std::string  bindAddress            = "0.0.0.0";
    std::string  port                   = "2700";
    size_t       threadCount            = []() -> size_t {
                                            auto hw = std::thread::hardware_concurrency();
                                            return hw > 0 ? static_cast<size_t>(hw) : 1;
                                          }();
    size_t       maxConnections         = 1000;
    size_t       maxQueueDepth          = 4096;
    size_t       maxRequestsPerConn     = 1000;
    size_t       maxBodyBytes           = 10ULL * 1024 * 1024;
    size_t       maxHeaderBytes         = 8 * 1024;
    size_t       maxUriBytes            = 2048;
    std::chrono::seconds headerTimeout  { 10 };
    std::chrono::seconds handlerTimeout { 30 };
    std::chrono::seconds shutdownTimeout{ 5  };
    Log::Level   logLevel               = Log::Level::info;
};
