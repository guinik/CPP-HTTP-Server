#pragma once
#include <string>
#include <unordered_map>

struct HTTPResponse {
	std::string version;
	std::string code;
	std::string reason;
	////
	std::unordered_map<std::string, std::string> headers;
	//for headers we might have way to many
	std::string body;
};