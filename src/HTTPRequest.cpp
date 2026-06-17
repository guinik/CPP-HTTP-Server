#include "HTTPRequest.hpp"
#include "HTTPErrors.hpp"
#include "StringUtils.hpp"
#include <format>
#include <stdexcept>

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

HTTPHead parseRawBytesHeadRequest(std::string_view raw)
{
    auto lfPos = raw.find("\r\n");
    if (lfPos == std::string_view::npos)
        throw BadRequestException("Malformed request line: missing CRLF");

    std::string_view firstLine = raw.substr(0, lfPos);

    auto sp1 = firstLine.find(' ');
    if (sp1 == std::string_view::npos)
        throw BadRequestException("Malformed request line");
    std::string_view method = firstLine.substr(0, sp1);

    auto sp2 = firstLine.find(' ', sp1 + 1);
    if (sp2 == std::string_view::npos)
        throw BadRequestException("Malformed request line");
    std::string_view path    = firstLine.substr(sp1 + 1, sp2 - sp1 - 1);
    std::string_view version = firstLine.substr(sp2 + 1);

    if (method.empty() || path.empty() || version.empty())
        throw BadRequestException("Malformed request line");

    for (unsigned char c : method) {
        if (isInvalidMethodChar(c))
            throw BadRequestException(
                std::format("Invalid character in method: 0x{:02x}", static_cast<unsigned>(c)));
    }

    if (path.size() > 2048)
        throw RequestUriTooLongException("Request-URI exceeds 2048 bytes");

    if (version != "HTTP/1.0" && version != "HTTP/1.1")
        throw HttpVersionNotSupportedException(
            std::format("Unsupported HTTP version: {}", version));

    CaseInsensitiveMap headerMap;
    size_t pos = lfPos + 2;

    while (pos < raw.size()) {
        auto nextCRLF = raw.find("\r\n", pos);
        if (nextCRLF == std::string_view::npos) break;

        std::string_view line = raw.substr(pos, nextCRLF - pos);
        if (line.empty()) break;

        auto colon = line.find(':');
        if (colon == std::string_view::npos)
            throw BadRequestException("Malformed header: missing colon");

        std::string_view key   = line.substr(0, colon);
        std::string_view value = line.substr(colon + 1);

        auto start = value.find_first_not_of(" \t");
        value = (start != std::string_view::npos)
                    ? value.substr(start, value.find_last_not_of(" \t") - start + 1)
                    : std::string_view{};

        // Only std::string copies happen here, at the point of storage.
        headerMap[std::string(key)] = std::string(value);
        pos = nextCRLF + 2;
    }

    return HTTPHead{
        .method      = std::string(method),
        .path        = std::string(path),
        .version     = std::string(version),
        .requestId   = {},
        .headers     = std::move(headerMap),
        .params      = {},
        .queryParams = {},
    };
}

HTTPBody parseRawBytesBodyRequest(const std::string& rawBody, const std::string& contentType) {
    return HTTPBody{
        .raw         = rawBody,
        .contentType = contentType
    };
}

HTTPRequest constructRequest(const HTTPHead& head, const HTTPBody& body)
{
    return HTTPRequest{
        .head = head,
        .body = body
    };
}
