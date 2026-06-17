#include <gtest/gtest.h>
#include "Cors.hpp"
#include "Utils.hpp"

// ── helpers ───────────────────────────────────────────────────────────────────

static HTTPRequest makeRequest(const std::string& method = "GET",
                               const std::string& origin  = "") {
    HTTPRequest req;
    req.head.method = method;
    if (!origin.empty())
        req.head.headers["Origin"] = origin;
    return req;
}

// ── makeCors ──────────────────────────────────────────────────────────────────

TEST(Cors, AllowedOriginPassesThrough) {
    CorsOptions opts{ {"https://example.com"}, {"GET"}, {"Content-Type"} };
    auto cors = makeCors(opts);

    auto req = makeRequest("GET", "https://example.com");
    HTTPResponse res;
    bool nextCalled = false;

    cors(req, res, [&nextCalled]() { nextCalled = true; });

    EXPECT_TRUE(nextCalled);
    EXPECT_EQ(res.headers.at("Access-Control-Allow-Origin"), "https://example.com");
}

TEST(Cors, BlockedOriginReturns403) {
    CorsOptions opts{ {"https://example.com"}, {"GET"}, {} };
    auto cors = makeCors(opts);

    auto req = makeRequest("GET", "https://evil.com");
    HTTPResponse res;
    bool nextCalled = false;

    cors(req, res, [&nextCalled]() { nextCalled = true; });

    EXPECT_FALSE(nextCalled);
    EXPECT_EQ(res.code, "403");
}

TEST(Cors, WildcardAllowsAnyOrigin) {
    CorsOptions opts{ {"*"}, {"GET"}, {} };
    auto cors = makeCors(opts);

    auto req = makeRequest("GET", "https://anyone.io");
    HTTPResponse res;
    bool nextCalled = false;

    cors(req, res, [&nextCalled]() { nextCalled = true; });

    EXPECT_TRUE(nextCalled);
    EXPECT_EQ(res.headers.at("Access-Control-Allow-Origin"), "*");
}

TEST(Cors, PreflightOptionsReturns204WithoutCallingNext) {
    CorsOptions opts{ {"https://example.com"}, {"GET", "POST"}, {"Content-Type"} };
    auto cors = makeCors(opts);

    auto req = makeRequest("OPTIONS", "https://example.com");
    HTTPResponse res;
    bool nextCalled = false;

    cors(req, res, [&nextCalled]() { nextCalled = true; });

    EXPECT_FALSE(nextCalled);
    EXPECT_EQ(res.code,   "204");
    EXPECT_EQ(res.reason, "No Content");
}

TEST(Cors, AllowedMethodsHeaderIsSet) {
    CorsOptions opts{ {"*"}, {"GET", "POST", "OPTIONS"}, {"Authorization"} };
    auto cors = makeCors(opts);

    auto req = makeRequest("GET", "https://x.com");
    HTTPResponse res;

    cors(req, res, []() {});

    EXPECT_EQ(res.headers.at("Access-Control-Allow-Methods"), "GET, POST, OPTIONS");
    EXPECT_EQ(res.headers.at("Access-Control-Allow-Headers"), "Authorization");
}

// ── parseJson middleware ───────────────────────────────────────────────────────

TEST(ParseJsonMiddleware, EmptyBodyCallsNext) {
    HTTPRequest req;
    HTTPResponse res;
    bool nextCalled = false;

    parseJson(req, res, [&nextCalled]() { nextCalled = true; });

    EXPECT_TRUE(nextCalled);
}

TEST(ParseJsonMiddleware, ValidJsonIsParsed) {
    HTTPRequest req;
    req.head.headers["Content-Type"] = "application/json";
    req.body.raw = R"({"name":"alex"})";
    HTTPResponse res;
    bool nextCalled = false;

    parseJson(req, res, [&nextCalled]() { nextCalled = true; });

    EXPECT_TRUE(nextCalled);
    EXPECT_EQ(req.body.json["name"], "alex");
}

TEST(ParseJsonMiddleware, InvalidJsonReturns400) {
    HTTPRequest req;
    req.head.headers["Content-Type"] = "application/json";
    req.body.raw = "{not valid json}";
    HTTPResponse res;
    bool nextCalled = false;

    parseJson(req, res, [&nextCalled]() { nextCalled = true; });

    EXPECT_FALSE(nextCalled);
    EXPECT_EQ(res.code, "400");
}

TEST(ParseJsonMiddleware, WrongContentTypeReturns415) {
    HTTPRequest req;
    req.head.headers["Content-Type"] = "text/plain";
    req.body.raw = "some text";
    HTTPResponse res;
    bool nextCalled = false;

    parseJson(req, res, [&nextCalled]() { nextCalled = true; });

    EXPECT_FALSE(nextCalled);
    EXPECT_EQ(res.code, "415");
}

TEST(ParseJsonMiddleware, NoContentTypeHeaderWithValidJsonParsesOk) {
    HTTPRequest req;
    req.body.raw = R"({"x":1})";
    HTTPResponse res;
    bool nextCalled = false;

    parseJson(req, res, [&nextCalled]() { nextCalled = true; });

    EXPECT_TRUE(nextCalled);
    EXPECT_EQ(req.body.json["x"], 1);
}
