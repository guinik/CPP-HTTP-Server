#pragma once
#include <functional>
#include <string>
#include <vector>
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"

using Next       = std::function<void()>;
using MiddleWare = std::function<void(HTTPRequest&, HTTPResponse&, Next)>;
using Handler    = std::function<void(const HTTPRequest&, HTTPResponse&)>;

struct Route {
    // Chain is composed once at RouteTrie::add() time.
    // At request time: zero allocations, one function call.
    std::function<void(HTTPRequest&, HTTPResponse&)> composedChain;
};

struct RouteMatch {
    Route*                   route = nullptr;
    bool                     pathFound = false;
    std::vector<std::string> allowedMethods;
};
