#pragma once
#include <string>
#include <unordered_map>
#include <algorithm>
#include "json.hpp"

struct CaseInsensitiveHash
{
    size_t operator()(const std::string& a) const {
        // we allocate a string per call, a per character hash with ^ might be reasonable to explore
        std::string lower = a;
        std::transform(lower.begin(), lower.end(), lower.begin(),
            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        return std::hash<std::string>{}(lower);
    }
};

struct CaseInsensitiveEqual
{
    bool operator()(const std::string& a, const std::string& b) const {
        return std::equal(a.begin(), a.end(), b.begin(), b.end(),
            [](unsigned char ca, unsigned char cb) {
                return std::tolower(ca) == std::tolower(cb);
            });
    }
};

using CaseInsensitiveMap = std::unordered_map<std::string, std::string,
                                               CaseInsensitiveHash, CaseInsensitiveEqual>;

struct HTTPHead {
    std::string method;
    std::string path;
    std::string version;
    std::string requestId;
    ////
    CaseInsensitiveMap headers;
    std::unordered_map<std::string, std::string> params;
    std::unordered_map<std::string, std::string> queryParams;
};

struct HTTPBody
{
    std::string raw;
    std::string contentType;
    nlohmann::json json = nlohmann::json::object();
};

struct HTTPRequest {
    HTTPHead head;
    HTTPBody body;
};

HTTPHead parseRawBytesHeadRequest(std::string_view rawRequest, size_t maxUriBytes = 2048);
HTTPBody parseRawBytesBodyRequest(const std::string& rawRequest, const std::string& contentType);
HTTPRequest constructRequest(const HTTPHead& head, const HTTPBody& body);
