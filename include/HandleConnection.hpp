#pragma once
#include <atomic>
#include "SocketGuard.hpp"
#include "HTTPRequest.hpp"
#include "HTTPSerializer.hpp"
#include "HTTPErrors.hpp"
#include "IRouter.hpp"

void HandleConnection(SocketGuard socket, IRouter& router, std::atomic_bool& running);
std::pair<std::string, std::string> ReadRequestHead(SocketGuard& socket);
std::string ReadRequestBody(SocketGuard& socket, size_t bodySize, std::string& leftover);
