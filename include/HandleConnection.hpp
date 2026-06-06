#pragma once

#include "SocketGuard.hpp"
#include "HTTPRequest.hpp"
void HandleConnection(SocketGuard socket);
std::string ReadRequest(SocketGuard& socket);