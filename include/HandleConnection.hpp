#pragma once
#include <atomic>
#include "SocketGuard.hpp"
#include "HTTPRequest.hpp"
#include "HTTPSerializer.hpp"
#include "Router.hpp"

void HandleConnection(SocketGuard socket, RadixTree& router, std::atomic_bool& running);
std::pair<std::string, std::string> ReadRequestHead(SocketGuard& socket);
std::string ReadRequestBody(SocketGuard& socket, size_t body, std::string& leftover);