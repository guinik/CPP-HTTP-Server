#pragma once
#include <string>
#include <vector>

inline std::vector<std::string> splitByDelimiter(const std::string& string, const std::string& delimiter)
{
	size_t posStart = 0;
	size_t posEnd;
	std::vector<std::string> result;
	while ((posEnd = string.find(delimiter, posStart)) != std::string::npos) {
		result.push_back(string.substr(posStart, posEnd - posStart));
		posStart = posEnd + delimiter.length();
	}
	result.push_back(string.substr(posStart));
	return result;
}
