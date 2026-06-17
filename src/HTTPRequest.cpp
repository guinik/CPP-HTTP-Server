#include "HTTPRequest.hpp"
#include "Router.hpp"
#include "StringUtils.hpp"
#include <format>
#include <stdexcept>
#include <vector>

// RFC 9110 §5.6.2 — characters that are NOT allowed in a method token.
static bool isInvalidMethodChar(unsigned char c)
{
    if (c < 0x21 || c == 0x7F) return true;  // control chars, space, DEL
    switch (c) {
        case '(': case ')': case '<': case '>': case '@':
        case ',': case ';': case ':': case '\\': case '"':
        case '/': case '[': case ']': case '?': case '=':
        case '{': case '}':
            return true;
        default:
            return false;
    }
}

HTTPHead parseRawBytesHeadRequest(const std::string& rawRequest) {
    auto pos = rawRequest.find("\r\n");

    std::string firstLine = rawRequest.substr(0, pos);
    std::vector<std::string> firstLineVector = splitByDelimiter(firstLine, " ");

    if (firstLineVector.size() != 3) {
        throw BadRequestException("Malformed request line");
    }

    const std::string& method  = firstLineVector[0];
    const std::string& path    = firstLineVector[1];
    const std::string& version = firstLineVector[2];

    // Validate method: non-empty token with no delimiter or control chars.
    if (method.empty())
        throw BadRequestException("Empty method");
    for (unsigned char c : method) {
        if (isInvalidMethodChar(c))
            throw BadRequestException(
                std::format("Invalid character in method: 0x{:02x}", static_cast<unsigned>(c)));
    }

    // Validate URL length before any further processing.
    if (path.size() > 2048)
        throw RequestUriTooLongException("Request-URI exceeds 2048 bytes");

    // Accept only HTTP/1.0 and HTTP/1.1; anything else gets 505.
    if (version != "HTTP/1.0" && version != "HTTP/1.1")
        throw HttpVersionNotSupportedException(
            std::format("Unsupported HTTP version: {}", version));

    size_t lastPos = pos + 2;
    size_t newPos;
    CaseInsensitiveMap headerMap;

    while ((newPos = rawRequest.find("\r\n", lastPos)) != std::string::npos) {

        auto newLine = rawRequest.substr(lastPos, newPos - lastPos);
        if (newLine.empty()) {
            lastPos = lastPos + 2;
            break;
        }

        size_t colonPosition = newLine.find(":");
        if (colonPosition == std::string::npos) {
            throw BadRequestException("Malformed header: missing colon");
        }
        std::string key = newLine.substr(0, colonPosition);
        std::string value = newLine.substr(colonPosition + 1, newLine.length() - (colonPosition + 1));
        size_t start = value.find_first_not_of(" \t");
        if (start != std::string::npos) {
            size_t end = value.find_last_not_of(" \t");
            value = value.substr(start, end - start + 1);
        } else {
            value = "";
        }

        headerMap[key] = value;
        lastPos = newPos + 2;
    }

    return HTTPHead{
         .method = method,
         .path = path,
         .version = version,
         .headers = headerMap,
    };
};

HTTPBody parseRawBytesBodyRequest(const std::string& rawBody, const std::string& contentType) {
    return HTTPBody{
            .raw = rawBody,
            .contentType = contentType
    };
};

HTTPRequest constructRequest(const HTTPHead& head, const HTTPBody& body)
{
    return HTTPRequest{
        .head = head,
        .body = body
    };
};
