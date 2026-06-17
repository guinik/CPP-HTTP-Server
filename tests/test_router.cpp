#include <gtest/gtest.h>
#include "Router.hpp"
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"

// Local test utility: compose and run a middleware chain in isolation.
static void applyRoute(const std::vector<MiddleWare>& middleware,
                       HTTPRequest& request, HTTPResponse& response,
                       const Handler& handler)
{
    std::function<void(HTTPRequest&, HTTPResponse&)> chain =
        [&handler](HTTPRequest& req, HTTPResponse& res) { handler(req, res); };
    for (int i = static_cast<int>(middleware.size()) - 1; i >= 0; --i) {
        auto next = chain;
        auto mw   = middleware[i];
        chain = [mw, next](HTTPRequest& req, HTTPResponse& res) {
            mw(req, res, [&req, &res, &next]() { next(req, res); });
        };
    }
    chain(request, response);
}

// ── stringDecode ──────────────────────────────────────────────────────────────

TEST(StringDecode, PercentEncodedSpace) {
    EXPECT_EQ(stringDecode("hello%20world"), "hello world");
}

TEST(StringDecode, PlusToSpace) {
    EXPECT_EQ(stringDecode("hello+world"), "hello world");
}

TEST(StringDecode, PercentEncodedSlash) {
    EXPECT_EQ(stringDecode("%2F"), "/");
}

TEST(StringDecode, NoEncoding) {
    EXPECT_EQ(stringDecode("unchanged"), "unchanged");
}

TEST(StringDecode, Mixed) {
    EXPECT_EQ(stringDecode("foo%3Dbar+baz"), "foo=bar baz");
}

TEST(StringDecode, InvalidPercentEncodingThrows) {
    EXPECT_THROW(stringDecode("bad%GGvalue"), BadRequestException);
}

TEST(StringDecode, TruncatedPercentSequencePassesThrough) {
    // % at end of string — not enough chars for a sequence, treated as literal
    EXPECT_EQ(stringDecode("a%"), "a%");
}

// ── RouteTrie::match ──────────────────────────────────────────────────────────

static HTTPRequest makeRequest(const std::string& method, const std::string& path) {
    HTTPRequest req;
    req.head.method = method;
    req.head.path   = path;
    return req;
}

TEST(Router, LiteralMatch) {
    RouteTrie tree;
    bool hit = false;
    tree.add("/users", "GET", {}, [&hit](const HTTPRequest&, HTTPResponse&) { hit = true; });

    auto req = makeRequest("GET", "/users");
    auto match = tree.match(req);

    EXPECT_NE(match.route, nullptr);
    EXPECT_TRUE(match.pathFound);
}

TEST(Router, NamedParamMatch) {
    RouteTrie tree;
    tree.add("/users/:id", "GET", {}, [](const HTTPRequest&, HTTPResponse&) {});

    auto req = makeRequest("GET", "/users/42");
    auto match = tree.match(req);

    EXPECT_NE(match.route, nullptr);
    EXPECT_EQ(req.head.params.at("id"), "42");
}

TEST(Router, WildcardMatch) {
    RouteTrie tree;
    tree.add("/public/*", "GET", {}, [](const HTTPRequest&, HTTPResponse&) {});

    auto req = makeRequest("GET", "/public/assets/style.css");
    auto match = tree.match(req);

    EXPECT_NE(match.route, nullptr);
    EXPECT_EQ(req.head.params.at("*"), "assets/style.css");
}

TEST(Router, LiteralBeatsParam) {
    RouteTrie tree;
    bool hitLiteral = false;
    bool hitParam   = false;
    tree.add("/users/me",  "GET", {}, [&hitLiteral](const HTTPRequest&, HTTPResponse&) { hitLiteral = true; });
    tree.add("/users/:id", "GET", {}, [&hitParam]  (const HTTPRequest&, HTTPResponse&) { hitParam   = true; });

    auto req = makeRequest("GET", "/users/me");
    auto match = tree.match(req);

    ASSERT_NE(match.route, nullptr);
    HTTPResponse res;
    match.route->composedChain(req, res);
    EXPECT_TRUE(hitLiteral);
    EXPECT_FALSE(hitParam);
}

TEST(Router, MethodNotAllowedReturnsPathFoundFlag) {
    RouteTrie tree;
    tree.add("/users", "GET", {}, [](const HTTPRequest&, HTTPResponse&) {});

    auto req = makeRequest("POST", "/users");
    auto match = tree.match(req);

    EXPECT_EQ(match.route, nullptr);
    EXPECT_TRUE(match.pathFound);
}

TEST(Router, QueryParamsParsed) {
    RouteTrie tree;
    tree.add("/users", "GET", {}, [](const HTTPRequest&, HTTPResponse&) {});

    auto req = makeRequest("GET", "/users?page=2&limit=10");
    tree.match(req);

    EXPECT_EQ(req.head.queryParams.at("page"),  "2");
    EXPECT_EQ(req.head.queryParams.at("limit"), "10");
}

TEST(Router, QueryParamsUrlDecoded) {
    RouteTrie tree;
    tree.add("/search", "GET", {}, [](const HTTPRequest&, HTTPResponse&) {});

    auto req = makeRequest("GET", "/search?q=hello%20world");
    tree.match(req);

    EXPECT_EQ(req.head.queryParams.at("q"), "hello world");
}

TEST(Router, DuplicateRegistrationThrows) {
    RouteTrie tree;
    tree.add("/users", "GET", {}, [](const HTTPRequest&, HTTPResponse&) {});
    EXPECT_THROW(
        tree.add("/users", "GET", {}, [](const HTTPRequest&, HTTPResponse&) {}),
        std::runtime_error
    );
}

TEST(Router, DuplicateMethodOnSamePathThrows) {
    // Two different methods on the same path are fine; duplicate method is not.
    RouteTrie tree;
    tree.add("/users", "GET",  {}, [](const HTTPRequest&, HTTPResponse&) {});
    tree.add("/users", "POST", {}, [](const HTTPRequest&, HTTPResponse&) {});
    EXPECT_THROW(
        tree.add("/users", "GET", {}, [](const HTTPRequest&, HTTPResponse&) {}),
        std::runtime_error
    );
}

TEST(Router, NoMatchReturnsNullRoute) {
    RouteTrie tree;
    tree.add("/users", "GET", {}, [](const HTTPRequest&, HTTPResponse&) {});

    auto req = makeRequest("GET", "/nonexistent");
    auto match = tree.match(req);

    EXPECT_EQ(match.route, nullptr);
    EXPECT_FALSE(match.pathFound);
}

// ── applyRoute ────────────────────────────────────────────────────────────────

TEST(ApplyRoute, NoMiddlewareCallsHandler) {
    HTTPRequest req;
    HTTPResponse res;
    Handler handler = [](const HTTPRequest&, HTTPResponse& r) { r.code = "200"; };

    applyRoute({}, req, res, handler);

    EXPECT_EQ(res.code, "200");
}

TEST(ApplyRoute, MiddlewareRunsBeforeHandler) {
    HTTPRequest req;
    HTTPResponse res;
    std::string order;

    MiddleWare mw = [&order](HTTPRequest&, HTTPResponse&, Next next) {
        order += "mw";
        next();
    };
    Handler handler = [&order](const HTTPRequest&, HTTPResponse& r) {
        order += "-handler";
        r.code = "200";
    };

    applyRoute({mw}, req, res, handler);

    EXPECT_EQ(order,    "mw-handler");
    EXPECT_EQ(res.code, "200");
}

TEST(ApplyRoute, MiddlewareShortCircuitBlocksHandler) {
    HTTPRequest req;
    HTTPResponse res;

    MiddleWare mw = [](HTTPRequest&, HTTPResponse& r, Next) {
        r.code = "403";
        // intentionally does not call next()
    };
    bool handlerCalled = false;
    Handler handler = [&handlerCalled](const HTTPRequest&, HTTPResponse&) {
        handlerCalled = true;
    };

    applyRoute({mw}, req, res, handler);

    EXPECT_EQ(res.code, "403");
    EXPECT_FALSE(handlerCalled);
}

TEST(ApplyRoute, MultipleMiddlewareChainedInOrder) {
    HTTPRequest req;
    HTTPResponse res;
    std::string order;

    MiddleWare mw1 = [&order](HTTPRequest&, HTTPResponse&, Next next) { order += "1"; next(); };
    MiddleWare mw2 = [&order](HTTPRequest&, HTTPResponse&, Next next) { order += "2"; next(); };
    Handler handler = [&order](const HTTPRequest&, HTTPResponse& r) { order += "H"; r.code = "200"; };

    applyRoute({mw1, mw2}, req, res, handler);

    EXPECT_EQ(order, "12H");
}
