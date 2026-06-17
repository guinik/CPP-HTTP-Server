#pragma once
#include <atomic>
#include <chrono>
#include "SocketGuard.hpp"
#include "HTTPRequest.hpp"
#include "HTTPSerializer.hpp"
#include "HTTPErrors.hpp"
#include "IRouter.hpp"
#include "Metrics.hpp"
#include "ServerConfig.hpp"

void HandleConnection(SocketGuard socket, const IRouter& router, std::atomic_bool& running,
                      Metrics& metrics,
                      const ServerConfig& config = ServerConfig{});

[[nodiscard]] std::string ReadRequestHead(
    SocketGuard& socket,
    std::string& leftover,
    size_t       maxHeaderBytes,
    std::chrono::seconds headerTimeout = std::chrono::seconds(10));

std::string ReadRequestBody(SocketGuard& socket, size_t bodySize, std::string& leftover);
