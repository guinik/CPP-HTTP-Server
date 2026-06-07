#pragma once

#include "SocketGuard.hpp"
#include "HTTPRequest.hpp"
#include "Router.hpp"
void HandleConnection(SocketGuard socket, RadixTree& router);
std::string ReadRequest(SocketGuard& socket);
std::string HTTPResponseToRawString(HTTPResponse& response);