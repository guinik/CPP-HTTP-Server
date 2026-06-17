#include <gtest/gtest.h>
#include "HTTPRequest.hpp"

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
