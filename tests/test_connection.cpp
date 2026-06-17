#include <gtest/gtest.h>
#include <atomic>
#include <thread>
#include <string>
#include <format>
#include <filesystem>
#include <fstream>

#include "HandleConnection.hpp"
#include "Router.hpp"

// ── platform socket boilerplate ───────────────────────────────────────────────
// SocketHandle / INVALID_HANDLE come from SocketGuard.hpp (via HandleConnection.hpp).

#ifdef _WIN32
#  include <winsock2.h>
#  include <ws2tcpip.h>
   using SockLen = int;
   static void rawClose(SocketHandle s) { closesocket(s); }
#else
#  include <sys/socket.h>
#  include <netinet/in.h>
#  include <arpa/inet.h>
#  include <unistd.h>
   using SockLen = socklen_t;
   static void rawClose(SocketHandle s) { ::close(s); }
#endif

// ── base fixture ──────────────────────────────────────────────────────────────

class ConnectionTest : public ::testing::Test {
protected:
    RouteTrie        router;
    std::atomic_bool running{ true };

    void SetUp() override {
#ifdef _WIN32
        WSADATA d{};
        ASSERT_EQ(WSAStartup(MAKEWORD(2, 2), &d), 0);
#endif
        running = true;

        router.add("/ok", "GET", {},
            [](const HTTPRequest&, HTTPResponse& res) {
                res.code = "200"; res.reason = "OK";
                res.body = "hello";
                res.headers["Content-Type"] = "text/plain";
            });

        router.add("/throws", "GET", {},
            [](const HTTPRequest&, HTTPResponse&) {
                throw std::runtime_error("deliberate");
            });

        router.add("/echo", "POST", {},
            [](const HTTPRequest& req, HTTPResponse& res) {
                res.code = "200"; res.reason = "OK";
                res.body = req.body.raw;
                res.headers["Content-Type"] = "text/plain";
            });
    }

    void TearDown() override {
#ifdef _WIN32
        WSACleanup();
#endif
    }

    // Binds a loopback listen socket with an OS-assigned port, connects a
    // client, accepts on the server side. Returns {serverSock, clientSock}.
    std::pair<SocketHandle, SocketHandle> makeConnection() {
        SocketHandle ls = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        EXPECT_NE(ls, INVALID_HANDLE);

        sockaddr_in addr{};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
        addr.sin_port        = 0; // OS picks a free port

        EXPECT_EQ(bind(ls, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);
        EXPECT_EQ(listen(ls, 1), 0);

        SockLen len = sizeof(addr);
        getsockname(ls, reinterpret_cast<sockaddr*>(&addr), &len);

        SocketHandle client = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        EXPECT_NE(client, INVALID_HANDLE);
        EXPECT_EQ(connect(client, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)), 0);

        SocketHandle server = accept(ls, nullptr, nullptr);
        EXPECT_NE(server, INVALID_HANDLE);
        rawClose(ls);
        return { server, client };
    }

    // Sends all bytes to sock; fails the test if the socket rejects data.
    void sendAll(SocketHandle sock, const std::string& s) {
        size_t off = 0;
        while (off < s.size()) {
            int n = ::send(sock, s.c_str() + off, static_cast<int>(s.size() - off), 0);
            if (n <= 0) return; // socket closed — test will fail on response check
            off += static_cast<size_t>(n);
        }
    }

    // Reads exactly one HTTP/1.1 response. Uses Content-Length to know where
    // the body ends, so the socket can be reused for keep-alive requests.
    std::string readOneResponse(SocketHandle sock) {
        std::string buf;
        char        tmp[1];

        while (buf.find("\r\n\r\n") == std::string::npos) {
            int n = recv(sock, tmp, 1, 0);
            if (n <= 0) return buf;
            buf += tmp[0];
            if (buf.size() > 32 * 1024) break; // safety cap
        }

        size_t headerEnd = buf.find("\r\n\r\n");
        if (headerEnd == std::string::npos) return buf;

        size_t bodyLen = 0;
        auto   clPos   = buf.find("Content-Length: ");
        if (clPos != std::string::npos && clPos < headerEnd) {
            size_t start = clPos + 16;
            size_t end   = buf.find("\r\n", start);
            if (end != std::string::npos)
                bodyLen = std::stoull(buf.substr(start, end - start));
        }

        size_t bodyRead = buf.size() - (headerEnd + 4);
        while (bodyRead < bodyLen) {
            int n = recv(sock, tmp, 1, 0);
            if (n <= 0) break;
            buf += tmp[0];
            ++bodyRead;
        }
        return buf;
    }

    // Starts HandleConnection on a background thread. The returned thread must
    // be joined by the caller (close clientSock first to trigger EOF).
    std::thread startWorker(SocketHandle serverSock) {
        return std::thread([this, serverSock]() {
            HandleConnection(SocketGuard(serverSock), router, running);
        });
    }
};

// ── route matching ────────────────────────────────────────────────────────────

TEST_F(ConnectionTest, SimpleGetReturns200) {
    auto [srv, cli] = makeConnection();
    auto t = startWorker(srv);

    sendAll(cli, "GET /ok HTTP/1.1\r\nConnection: close\r\n\r\n");
    auto resp = readOneResponse(cli);
    rawClose(cli);
    t.join();

    EXPECT_NE(resp.find("HTTP/1.1 200 OK"), std::string::npos);
    EXPECT_NE(resp.find("hello"),            std::string::npos);
}

TEST_F(ConnectionTest, UnknownPathReturns404) {
    auto [srv, cli] = makeConnection();
    auto t = startWorker(srv);

    sendAll(cli, "GET /nonexistent HTTP/1.1\r\nConnection: close\r\n\r\n");
    auto resp = readOneResponse(cli);
    rawClose(cli);
    t.join();

    EXPECT_NE(resp.find("HTTP/1.1 404"), std::string::npos);
}

TEST_F(ConnectionTest, WrongMethodReturns405) {
    auto [srv, cli] = makeConnection();
    auto t = startWorker(srv);

    sendAll(cli, "POST /ok HTTP/1.1\r\nConnection: close\r\n\r\n");
    auto resp = readOneResponse(cli);
    rawClose(cli);
    t.join();

    EXPECT_NE(resp.find("HTTP/1.1 405"), std::string::npos);
}

TEST_F(ConnectionTest, HandlerThrowingReturns500) {
    auto [srv, cli] = makeConnection();
    auto t = startWorker(srv);

    sendAll(cli, "GET /throws HTTP/1.1\r\nConnection: close\r\n\r\n");
    auto resp = readOneResponse(cli);
    rawClose(cli);
    t.join();

    EXPECT_NE(resp.find("HTTP/1.1 500"), std::string::npos);
}

// ── request body ──────────────────────────────────────────────────────────────

TEST_F(ConnectionTest, PostBodyIsReceivedCorrectly) {
    auto [srv, cli] = makeConnection();
    auto t = startWorker(srv);

    std::string body = "hello body";
    std::string req  = std::format(
        "POST /echo HTTP/1.1\r\n"
        "Content-Length: {}\r\n"
        "Connection: close\r\n"
        "\r\n{}",
        body.size(), body);

    sendAll(cli, req);
    auto resp = readOneResponse(cli);
    rawClose(cli);
    t.join();

    EXPECT_NE(resp.find("HTTP/1.1 200"), std::string::npos);
    EXPECT_NE(resp.find("hello body"),   std::string::npos);
}

// ── Content-Length edge cases ─────────────────────────────────────────────────

TEST_F(ConnectionTest, NegativeContentLengthReturns400) {
    // stoi("-1") = -1; detected before size_t conversion → BadRequestException → 400
    auto [srv, cli] = makeConnection();
    auto t = startWorker(srv);

    sendAll(cli, "POST /echo HTTP/1.1\r\nContent-Length: -1\r\nConnection: close\r\n\r\n");
    auto resp = readOneResponse(cli);
    rawClose(cli);
    t.join();

    EXPECT_NE(resp.find("HTTP/1.1 400"), std::string::npos);
}

TEST_F(ConnectionTest, OverflowContentLengthReturns500) {
    // Value exceeds INT_MAX → stoi throws std::out_of_range → runtime_error → 500
    auto [srv, cli] = makeConnection();
    auto t = startWorker(srv);

    sendAll(cli, "POST /echo HTTP/1.1\r\nContent-Length: 99999999999\r\nConnection: close\r\n\r\n");
    auto resp = readOneResponse(cli);
    rawClose(cli);
    t.join();

    EXPECT_NE(resp.find("HTTP/1.1 500"), std::string::npos);
}

// ── keep-alive / connection lifecycle ─────────────────────────────────────────

TEST_F(ConnectionTest, HTTP11DefaultsToKeepAlive) {
    auto [srv, cli] = makeConnection();
    auto t = startWorker(srv);

    // First request — no Connection header; HTTP/1.1 default is keep-alive.
    sendAll(cli, "GET /ok HTTP/1.1\r\n\r\n");
    auto resp1 = readOneResponse(cli);
    EXPECT_NE(resp1.find("Connection: keep-alive"), std::string::npos);

    // Socket still open — second request must succeed on the same connection.
    sendAll(cli, "GET /ok HTTP/1.1\r\nConnection: close\r\n\r\n");
    auto resp2 = readOneResponse(cli);
    EXPECT_NE(resp2.find("HTTP/1.1 200"), std::string::npos);

    rawClose(cli);
    t.join();
}

TEST_F(ConnectionTest, HTTP10DefaultsToClose) {
    // HTTP/1.0 without a Connection header must produce Connection: close.
    auto [srv, cli] = makeConnection();
    auto t = startWorker(srv);

    sendAll(cli, "GET /ok HTTP/1.0\r\n\r\n");
    auto resp = readOneResponse(cli);
    rawClose(cli);
    t.join();

    EXPECT_NE(resp.find("Connection: close"), std::string::npos);
}

TEST_F(ConnectionTest, ConnectionCloseRespected) {
    auto [srv, cli] = makeConnection();
    auto t = startWorker(srv);

    sendAll(cli, "GET /ok HTTP/1.1\r\nConnection: close\r\n\r\n");
    auto resp = readOneResponse(cli);
    rawClose(cli);
    t.join();

    EXPECT_NE(resp.find("Connection: close"), std::string::npos);
}

TEST_F(ConnectionTest, ConnectionKeepAliveIsCaseInsensitive) {
    // RFC 7230 §6.1: connection option tokens are case-insensitive.
    // "Keep-Alive" must be treated identically to "keep-alive".
    auto [srv, cli] = makeConnection();
    auto t = startWorker(srv);

    sendAll(cli, "GET /ok HTTP/1.1\r\nConnection: Keep-Alive\r\n\r\n");
    auto resp1 = readOneResponse(cli);
    EXPECT_NE(resp1.find("Connection: keep-alive"), std::string::npos);

    // If the old bug were present the server would have closed the connection
    // after resp1; this second request would fail or return garbage.
    sendAll(cli, "GET /ok HTTP/1.1\r\nConnection: close\r\n\r\n");
    auto resp2 = readOneResponse(cli);
    EXPECT_NE(resp2.find("HTTP/1.1 200"), std::string::npos);

    rawClose(cli);
    t.join();
}

// ── path traversal ────────────────────────────────────────────────────────────

class StaticFileTest : public ConnectionTest {
protected:
    std::filesystem::path tmpRoot;

    void SetUp() override {
        ConnectionTest::SetUp();

        namespace fs = std::filesystem;
        tmpRoot = fs::temp_directory_path() / "http_srv_static_test";
        fs::create_directories(tmpRoot / "public");
        std::ofstream(tmpRoot / "public" / "hello.txt") << "file content";
        // A file that lives OUTSIDE public — must never be served.
        std::ofstream(tmpRoot / "secret.txt") << "secret data";

        fs::path publicRoot = fs::canonical(tmpRoot / "public");

        // Register a /files/* route using the same path-traversal logic
        // as the production static-file handler, rooted at publicRoot.
        router.add("/files/*", "GET", {},
            [publicRoot](const HTTPRequest& req, HTTPResponse& res) {
                namespace fs = std::filesystem;

                fs::path requested;
                try {
                    requested = fs::canonical(publicRoot / req.head.params.at("*"));
                } catch (const fs::filesystem_error&) {
                    res.code = "404"; res.reason = "Not Found";
                    res.body = "File not found";
                    return;
                }

                auto [rootEnd, _] = std::mismatch(
                    publicRoot.begin(), publicRoot.end(), requested.begin());
                if (rootEnd != publicRoot.end()) {
                    res.code = "403"; res.reason = "Forbidden";
                    return;
                }

                std::ifstream f(requested, std::ios::binary);
                if (!f.is_open()) {
                    res.code = "404"; res.reason = "Not Found";
                    return;
                }
                res.code = "200"; res.reason = "OK";
                res.body = std::string(std::istreambuf_iterator<char>(f), {});
                res.headers["Content-Type"] = "text/plain";
            });
    }

    void TearDown() override {
        std::filesystem::remove_all(tmpRoot);
        ConnectionTest::TearDown();
    }
};

TEST_F(StaticFileTest, LegitimateFileIsServed) {
    auto [srv, cli] = makeConnection();
    auto t = startWorker(srv);

    sendAll(cli, "GET /files/hello.txt HTTP/1.1\r\nConnection: close\r\n\r\n");
    auto resp = readOneResponse(cli);
    rawClose(cli);
    t.join();

    EXPECT_NE(resp.find("HTTP/1.1 200"), std::string::npos);
    EXPECT_NE(resp.find("file content"), std::string::npos);
}

TEST_F(StaticFileTest, NonExistentFileReturns404) {
    auto [srv, cli] = makeConnection();
    auto t = startWorker(srv);

    sendAll(cli, "GET /files/missing.txt HTTP/1.1\r\nConnection: close\r\n\r\n");
    auto resp = readOneResponse(cli);
    rawClose(cli);
    t.join();

    EXPECT_NE(resp.find("HTTP/1.1 404"), std::string::npos);
}

TEST_F(StaticFileTest, DotDotTraversalToExistingFileReturns403) {
    // The secret file EXISTS on disk at tmpRoot/secret.txt.
    // canonical() resolves "../secret.txt" to that path, and the prefix
    // check detects it is outside publicRoot → 403.
    auto [srv, cli] = makeConnection();
    auto t = startWorker(srv);

    sendAll(cli, "GET /files/../secret.txt HTTP/1.1\r\nConnection: close\r\n\r\n");
    auto resp = readOneResponse(cli);
    rawClose(cli);
    t.join();

    EXPECT_NE(resp.find("HTTP/1.1 403"), std::string::npos);
    EXPECT_EQ(resp.find("secret data"), std::string::npos);
}

TEST_F(StaticFileTest, DotDotTraversalToNonExistentPathReturns404) {
    // Path resolves outside public but target does not exist → canonical()
    // throws → 404, not a 200 leak.
    auto [srv, cli] = makeConnection();
    auto t = startWorker(srv);

    sendAll(cli, "GET /files/../../nonexistent_file HTTP/1.1\r\nConnection: close\r\n\r\n");
    auto resp = readOneResponse(cli);
    rawClose(cli);
    t.join();

    EXPECT_EQ(resp.find("HTTP/1.1 200"), std::string::npos);
}
