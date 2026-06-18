#pragma once
#include <string>
#include <vector>
#include "HTTPErrors.hpp"
#include <charconv>

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

inline std::string stringDecode(std::string input)
{
    auto isHex = [](char c) {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    };
    std::string result;
    for (size_t i = 0; i < input.size(); i++) {
        if (input[i] == '%' && i + 2 < input.size()) {
            if (!isHex(input[i + 1]) || !isHex(input[i + 2]))
                throw BadRequestException("Invalid percent-encoding in URL");
            int hexValue;
            auto [ptr, ec] = std::from_chars(input.data() + i + 1, input.data() + i + 3, hexValue, 16);
            if (ec == std::errc()) {
                result += static_cast<char>(hexValue);
                i += 2;
            }
            else{
                throw std::runtime_error("Couldnt decode the string");
            }
        } else if (input[i] == '+') {
            result += ' ';
        } else {
            result += input[i];
        }
    }
    return result;
}
