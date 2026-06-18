#pragma once
#include "RouteTypes.hpp"

struct CorsOptions{
    std::vector<std::string> allowedOrigins;
    std::vector<std::string> allowedMethods;
    std::vector<std::string> allowedHeaders;
};

MiddleWare makeCors(CorsOptions corsOpt);
