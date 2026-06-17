#include <gtest/gtest.h>
#include "HTTPRequest.hpp"
#include "Router.hpp"
#include "StringUtils.hpp"
// ── splitByDelimiter ──────────────────────────────────────────────────────────

TEST(SplitByDelimiter, BasicSlash) {
    auto result = splitByDelimiter("/users/42", "/");
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[0], "");
    EXPECT_EQ(result[1], "users");
    EXPECT_EQ(result[2], "42");
}

TEST(SplitByDelimiter, NoDelimiterPresent) {
    auto result = splitByDelimiter("hello", "/");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], "hello");
}

TEST(SplitByDelimiter, TrailingDelimiter) {
    auto result = splitByDelimiter("a/b/", "/");
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[2], "");
}

TEST(SplitByDelimiter, MultiCharDelimiter) {
    auto result = splitByDelimiter("a\r\nb\r\nc", "\r\n");
    ASSERT_EQ(result.size(), 3u);
    EXPECT_EQ(result[1], "b");
}

TEST(SplitByDelimiter, EmptyString) {
    auto result = splitByDelimiter("", "/");
    ASSERT_EQ(result.size(), 1u);
    EXPECT_EQ(result[0], "");
}

// ── parseRawBytesHeadRequest ──────────────────────────────────────────────────

TEST(ParseHead, SimpleGet) {
    std::string raw = "GET /users HTTP/1.1\r\n\r\n";
    auto head = parseRawBytesHeadRequest(raw);
    EXPECT_EQ(head.method,  "GET");
    EXPECT_EQ(head.path,    "/users");
    EXPECT_EQ(head.version, "HTTP/1.1");
}

TEST(ParseHead, HeadersAreParsed) {
    std::string raw =
        "GET /users HTTP/1.1\r\n"
        "Host: localhost\r\n"
        "Content-Length: 0\r\n"
        "\r\n";
    auto head = parseRawBytesHeadRequest(raw);
    EXPECT_EQ(head.headers.at("Host"),           "localhost");
    EXPECT_EQ(head.headers.at("Content-Length"), "0");
}

TEST(ParseHead, HeaderLookupIsCaseInsensitive) {
    std::string raw =
        "POST /data HTTP/1.1\r\n"
        "Content-Type: application/json\r\n"
        "\r\n";
    auto head = parseRawBytesHeadRequest(raw);
    EXPECT_EQ(head.headers.at("content-type"), "application/json");
    EXPECT_EQ(head.headers.at("CONTENT-TYPE"), "application/json");
}

TEST(ParseHead, HeaderValueLeadingSpaceStripped) {
    std::string raw =
        "GET / HTTP/1.1\r\n"
        "Authorization:   Bearer token123\r\n"
        "\r\n";
    auto head = parseRawBytesHeadRequest(raw);
    EXPECT_EQ(head.headers.at("Authorization"), "Bearer token123");
}

TEST(ParseHead, MalformedFirstLineThrows) {
    std::string raw = "BADREQUEST\r\n\r\n";
    EXPECT_THROW(parseRawBytesHeadRequest(raw), std::runtime_error);
}

TEST(ParseHead, HeaderValueWithColonIsPreserved) {
    // e.g. Date header — value itself contains colons
    std::string raw =
        "GET / HTTP/1.1\r\n"
        "Date: Mon, 01 Jan 2024 12:00:00 GMT\r\n"
        "\r\n";
    auto head = parseRawBytesHeadRequest(raw);
    EXPECT_EQ(head.headers.at("Date"), "Mon, 01 Jan 2024 12:00:00 GMT");
}

TEST(ParseHead, EmptyHeaderValueIsAccepted) {
    std::string raw =
        "GET / HTTP/1.1\r\n"
        "X-Empty:\r\n"
        "\r\n";
    auto head = parseRawBytesHeadRequest(raw);
    EXPECT_EQ(head.headers.at("X-Empty"), "");
}

TEST(ParseHead, MultipleHeadersAllParsed) {
    std::string raw =
        "POST /submit HTTP/1.1\r\n"
        "Host: example.com\r\n"
        "Content-Type: application/json\r\n"
        "Content-Length: 13\r\n"
        "Authorization: Bearer tok\r\n"
        "\r\n";
    auto head = parseRawBytesHeadRequest(raw);
    EXPECT_EQ(head.method, "POST");
    EXPECT_EQ(head.headers.at("Host"),           "example.com");
    EXPECT_EQ(head.headers.at("Content-Type"),   "application/json");
    EXPECT_EQ(head.headers.at("Content-Length"), "13");
    EXPECT_EQ(head.headers.at("Authorization"),  "Bearer tok");
}

// ── parseRawBytesBodyRequest ──────────────────────────────────────────────────

// ── request-line validation ───────────────────────────────────────────────────

TEST(ParseHead, Http10IsAccepted) {
    std::string raw = "GET /ok HTTP/1.0\r\n\r\n";
    auto head = parseRawBytesHeadRequest(raw);
    EXPECT_EQ(head.version, "HTTP/1.0");
}

TEST(ParseHead, UnsupportedHttpVersionThrows505) {
    std::string raw = "GET /path HTTP/2.0\r\n\r\n";
    EXPECT_THROW(parseRawBytesHeadRequest(raw), HttpVersionNotSupportedException);
}

TEST(ParseHead, HttpVersionHTTP0_9Throws505) {
    std::string raw = "GET /path HTTP/0.9\r\n\r\n";
    EXPECT_THROW(parseRawBytesHeadRequest(raw), HttpVersionNotSupportedException);
}

TEST(ParseHead, MethodWithTabCharThrows400) {
    // Tab is not a valid token char; must produce BadRequestException, not a 500.
    std::string raw = "G\tET / HTTP/1.1\r\n\r\n";
    EXPECT_THROW(parseRawBytesHeadRequest(raw), BadRequestException);
}

TEST(ParseHead, MethodWithColonThrows400) {
    std::string raw = "GE:T / HTTP/1.1\r\n\r\n";
    EXPECT_THROW(parseRawBytesHeadRequest(raw), BadRequestException);
}

TEST(ParseHead, UriTooLongThrows414) {
    // 2049 chars — just over the 2048-byte limit.
    std::string longPath = "/" + std::string(2048, 'a');
    std::string raw = "GET " + longPath + " HTTP/1.1\r\n\r\n";
    EXPECT_THROW(parseRawBytesHeadRequest(raw), RequestUriTooLongException);
}

TEST(ParseHead, UriAtLimitIsAccepted) {
    // Exactly 2048 chars — must not throw.
    std::string path = "/" + std::string(2047, 'a');
    std::string raw = "GET " + path + " HTTP/1.1\r\n\r\n";
    EXPECT_NO_THROW(parseRawBytesHeadRequest(raw));
}

// ── parseRawBytesBodyRequest ──────────────────────────────────────────────────

TEST(ParseBody, StoresRawBodyAndContentType) {
    std::string body = "{\"key\":\"value\"}";
    auto result = parseRawBytesBodyRequest(body, "application/json");
    EXPECT_EQ(result.raw,         body);
    EXPECT_EQ(result.contentType, "application/json");
}

TEST(ParseBody, EmptyBody) {
    auto result = parseRawBytesBodyRequest("", "");
    EXPECT_TRUE(result.raw.empty());
}

TEST(ParseBody, BinaryBodyStoredVerbatim) {
    std::string body = "raw\x00\x01\x02 data";
    body[3] = '\0';  // embed a null byte
    auto result = parseRawBytesBodyRequest(body, "application/octet-stream");
    EXPECT_EQ(result.raw.size(), body.size());
    EXPECT_EQ(result.raw, body);
}
