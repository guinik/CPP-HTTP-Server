#pragma once
#include <string>
#include <unordered_map>
#include "json.hpp"
#include <algorithm>
struct CaseInsensitiveHash 
{
	size_t operator()(const std::string& a) const{
		std::string lower = a;
		std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
		return std::hash<std::string>{}(lower);
	}
};

struct CaseInsensitiveEqual	 
{
	bool operator()(const std::string& a, const std::string& b) const {
		return std::equal(a.begin(), a.end(), b.begin(), b.end(),
			[](char ca, char cb) {
				return std::tolower(ca) == std::tolower(cb);

			});
	};
};
using CaseInsensitiveMap = std::unordered_map<std::string, std::string, CaseInsensitiveHash, CaseInsensitiveEqual>;

struct HTTPHead {
	std::string method;
	std::string path;
	std::string version;
	////
	CaseInsensitiveMap headers;
	CaseInsensitiveMap params;
	CaseInsensitiveMap queryParams;
};
struct HTTPBody
{
	std::string raw;
	std::string contentType;
	nlohmann::json json = nlohmann::json::object();;
};

struct HTTPRequest {
	HTTPHead head;
	HTTPBody body;
};

HTTPHead parseRawBytesHeadRequest(const std::string& rawRequest);
HTTPBody parseRawBytesBodyRequest(const std::string& rawRequest, const std::string& contentType);
HTTPRequest constructRequest(const HTTPHead& head, const HTTPBody& body);

std::vector<std::string> splitByDelimiter(const std::string& string, const std::string& delimiter);