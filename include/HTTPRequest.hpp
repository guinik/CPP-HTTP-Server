#pragma once
#include <string>
#include <unordered_map>


struct HTTPHead {
	std::string method;
	std::string path;
	std::string version;
	////
	std::unordered_map<std::string, std::string> headers;
	std::unordered_map<std::string, std::string> params;
};

struct HTTPRequest {
	HTTPHead head;
	std::string body;
};

using HTTPBody = std::string;
//struct HTTPRequest {
//	std::string method;
//	std::string path;
//	std::string version;
	////
//	std::unordered_map<std::string, std::string> headers;
//	std::unordered_map<std::string, std::string> params;
	//for headers we might have way to many
//	std::string body;
//};
HTTPHead parseRawBytesHeadRequest(const std::string& rawRequest);
HTTPBody parseRawBytesBodyRequest(const std::string& rawRequest);
HTTPRequest constructRequest(const HTTPHead& head, const HTTPBody& body);

std::vector<std::string> splitByDelimiter(const std::string& string, const std::string& delimiter);