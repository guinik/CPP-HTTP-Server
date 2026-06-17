#pragma once
#include <functional>
#include <string>
#include <vector>
#include <unordered_map>
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
    const Route*                                 route = nullptr;
    bool                                         pathFound = false;
    std::vector<std::string>                     allowedMethods;
    // Populated by RouteTrie::match(); applied to request.head by HandleConnection
    // so that user handlers continue to read req.head.params / req.head.queryParams.
    std::unordered_map<std::string, std::string> params;
    std::unordered_map<std::string, std::string> queryParams;
};
