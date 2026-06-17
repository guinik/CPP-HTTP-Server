#pragma once
#include "RouteTypes.hpp"

class IRouter {
public:
    virtual ~IRouter() = default;
    virtual RouteMatch match(const HTTPRequest& request) const = 0;
};
