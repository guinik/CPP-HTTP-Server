#pragma once
#include "RouteTypes.hpp"

class IRouter {
public:
    virtual ~IRouter() = default;
    virtual RouteMatch match(HTTPRequest& request) = 0;
};
