#pragma once
#include <string>
#include <unordered_map>

struct HTTPRequest {
	std::string method;
	std::string path;
	std::string version;
	////
	std::unordered_map<std::string, std::string> headers;
	std::unordered_map<std::string, std::string> params;
	//for headers we might have way to many
	std::string body;
};


HTTPRequest parseRawBytesRequest(const std::string& rawRequest);

std::vector<std::string> splitByDelimiter(const std::string& string, const std::string& delimiter);