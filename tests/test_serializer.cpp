#include <gtest/gtest.h>
#include "HTTPSerializer.hpp"

static HTTPResponse makeResponse(const std::string& code, const std::string& reason,
                                 const std::string& body = "") {
    HTTPResponse r;
    r.code   = code;
    r.reason = reason;
    r.body   = body;
    return r;
}

// ── status line ───────────────────────────────────────────────────────────────

TEST(Serializer, StatusLineFormat) {
    auto r = makeResponse("200", "OK");
    auto raw = HTTPResponseToRawString(r);
    EXPECT_TRUE(raw.starts_with("HTTP/1.1 200 OK\r\n"));
}

TEST(Serializer, StatusLine404) {
    auto r = makeResponse("404", "Not Found");
    auto raw = HTTPResponseToRawString(r);
    EXPECT_TRUE(raw.starts_with("HTTP/1.1 404 Not Found\r\n"));
}

// ── Content-Length ────────────────────────────────────────────────────────────

TEST(Serializer, ContentLengthAddedWhenBodyPresent) {
    auto r = makeResponse("200", "OK", "hello");
    auto raw = HTTPResponseToRawString(r);
    EXPECT_NE(raw.find("Content-Length: 5\r\n"), std::string::npos);
}

TEST(Serializer, ContentLengthMatchesBodyBytes) {
    std::string body = "{\"key\":\"value\"}";
    auto r = makeResponse("200", "OK", body);
    auto raw = HTTPResponseToRawString(r);
    std::string expected = "Content-Length: " + std::to_string(body.size()) + "\r\n";
    EXPECT_NE(raw.find(expected), std::string::npos);
}

TEST(Serializer, NoContentLengthWhenBodyEmpty) {
    auto r = makeResponse("204", "No Content");
    auto raw = HTTPResponseToRawString(r);
    EXPECT_EQ(raw.find("Content-Length"), std::string::npos);
}

// ── Date header ───────────────────────────────────────────────────────────────

TEST(Serializer, DateHeaderIsPresent) {
    auto r = makeResponse("200", "OK");
    auto raw = HTTPResponseToRawString(r);
    EXPECT_NE(raw.find("Date: "), std::string::npos);
}

// ── custom headers ────────────────────────────────────────────────────────────

TEST(Serializer, CustomHeadersAppear) {
    auto r = makeResponse("200", "OK", "{}");
    r.headers["Content-Type"] = "application/json";
    auto raw = HTTPResponseToRawString(r);
    EXPECT_NE(raw.find("Content-Type: application/json\r\n"), std::string::npos);
}

// ── body ─────────────────────────────────────────────────────────────────────

TEST(Serializer, BodyAppearsAfterBlankLine) {
    auto r = makeResponse("200", "OK", "hello world");
    auto raw = HTTPResponseToRawString(r);
    auto headerEnd = raw.find("\r\n\r\n");
    ASSERT_NE(headerEnd, std::string::npos);
    EXPECT_EQ(raw.substr(headerEnd + 4), "hello world");
}

TEST(Serializer, EmptyBodyProducesBlankLineAtEnd) {
    auto r = makeResponse("404", "Not Found");
    auto raw = HTTPResponseToRawString(r);
    EXPECT_TRUE(raw.ends_with("\r\n\r\n"));
}

// ── CRLF injection prevention ─────────────────────────────────────────────────
// The threat is a new header line being injected. After stripping, \r\n can
// no longer appear inside a field, so the injected text cannot start a new
// line. We assert that "\r\nX-Injected" is absent, not that the literal text
// is absent (it may appear as a run-on substring in the status line, but that
// is harmless).

TEST(Serializer, CRLFInStatusCodeIsStripped) {
    auto r = makeResponse("200\r\nX-Injected: evil", "OK");
    auto raw = HTTPResponseToRawString(r);
    EXPECT_EQ(raw.find("\r\nX-Injected"), std::string::npos);
    EXPECT_NE(raw.find("200"), std::string::npos);
}

TEST(Serializer, CRLFInReasonIsStripped) {
    auto r = makeResponse("200", "OK\r\nX-Injected: evil");
    auto raw = HTTPResponseToRawString(r);
    EXPECT_EQ(raw.find("\r\nX-Injected"), std::string::npos);
}

TEST(Serializer, CRLFInHeaderValueIsStripped) {
    auto r = makeResponse("200", "OK");
    r.headers["X-Custom"] = "value\r\nX-Injected: evil";
    auto raw = HTTPResponseToRawString(r);
    EXPECT_EQ(raw.find("\r\nX-Injected"), std::string::npos);
    EXPECT_NE(raw.find("X-Custom: value"), std::string::npos);
}
