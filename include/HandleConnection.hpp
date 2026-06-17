#pragma once
#include <atomic>
#include <chrono>
#include "SocketGuard.hpp"
#include "HTTPRequest.hpp"
#include "HTTPSerializer.hpp"
#include "HTTPErrors.hpp"
#include "IRouter.hpp"

void HandleConnection(SocketGuard socket, IRouter& router, std::atomic_bool& running,
                      size_t               maxRequestsPerConnection = 1000,
                      std::chrono::seconds headerReadTimeout        = std::chrono::seconds(10),
                      std::chrono::seconds handlerTimeout           = std::chrono::seconds(30));

[[nodiscard]] std::pair<std::string, std::string> ReadRequestHead(
    SocketGuard& socket,
    std::chrono::seconds headerTimeout = std::chrono::seconds(10));

std::string ReadRequestBody(SocketGuard& socket, size_t bodySize, std::string& leftover);
