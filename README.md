# C++ HTTP Server

A lightweight HTTP/1.1 server built from scratch in C++20 for Windows. No frameworks, no dependencies beyond the Windows SDK and a vendored JSON header.

## Features

- **Radix tree router** - O(path depth) matching with named parameters (`:id`), wildcard segments (`*`), and DFS backtracking for correct specificity
- **HTTP/1.1 keep-alive** - persistent connections reused across multiple requests; respects `Connection: close` and HTTP/1.0 defaults
- **Thread pool** - fixed-size pool sized to `hardware_concurrency()`; no unbounded thread spawning
- **Middleware pipeline** - per-route chain with `Next` continuation; short-circuit by not calling `next()`
- **Static file serving** - wildcard routes serve files from disk with MIME type detection
- **CORS** - configurable origins, methods, and headers; handles `OPTIONS` preflight automatically
- **JSON body parsing** - `Content-Type: application/json` bodies parsed automatically into `req.body.json` (nlohmann/json)
- **Query parameter parsing** - `?key=value` pairs URL-decoded into `req.head.queryParams`
- **Case-insensitive headers** - header lookup follows RFC 7230; `Content-Type` and `content-type` are the same key
- **Request logging** - built-in middleware logs method, path, status, and latency
- **Graceful shutdown** - CTRL-C sets a stop flag; accept loop drains and exits cleanly
- **RAII everywhere** - `SocketGuard`, `AddrInfoGuard`, and `WinSocketGuard` ensure clean resource cleanup on every code path

## Architecture

```
main.cpp
  └── parses port from argv (default 2700)
  └── registers routes via addUserRoutes()
  └── creates ThreadPool (hardware_concurrency threads)
  └── creates WSAHandler → calls run()

WSAHandler::run()
  └── sets up TCP listen socket
  └── accept loop → enqueues HandleConnection on ThreadPool
  └── polls g_running flag; exits on SIGINT

HandleConnection()  [runs on thread pool]
  └── keep-alive loop:
        └── ReadRequestHead() → ReadRequestBody() → HTTPRequest
        └── RadixTree::match() → node (populates req.head.params + queryParams)
        └── method lookup → 404 / 405 / applyRoute()
        └── applyRoute() → middleware chain → handler → HTTPResponse
        └── HTTPResponseToRawString() → sendData()
        └── break if Connection: close
```

## Project Structure

```
include/
  AddrInfo.hpp       - RAII wrapper for getaddrinfo result
  SocketGuard.hpp    - RAII wrapper for SOCKET handle
  WSA.hpp            - WSAHandler class + WinSocketGuard
  HTTPRequest.hpp    - HTTPRequest / HTTPHead / HTTPBody structs + parser declarations
  HTTPResponse.hpp   - HTTPResponse struct
  Router.hpp         - RadixTree, RadixTreeNode, Route, MiddleWare, Handler types
  HandleConnection.hpp
  Cors.hpp           - makeCors() middleware factory
  Utils.hpp          - parseJson + requestLogger middleware
  UserRoutes.hpp     - addUserRoutes() declaration
  json.hpp           - nlohmann/json (single-header, vendored)

src/
  main.cpp              - entry point, port config, signal handler
  WSA.cpp               - listen loop, thread pool dispatch
  HandleConnection.cpp  - connection lifecycle, keep-alive loop, response serializer
  HTTPRequest.cpp       - raw bytes → HTTPRequest (head, body, query params)
  Router.cpp            - RadixTree::add(), match(), dfsFindMatch(), applyRoute()
  UserRoutes.cpp        - user-defined route handlers
  ThreadPool.cpp        - fixed-size thread pool
  SocketGuard.cpp       - send, recv, bind, listen, accept wrappers
```

## Building

**Requirements:** Windows, CMake 3.20+, MSVC (C++20)

```bash
cmake -S . -B build
cmake --build build
```

## Running

```bash
./build/Debug/HTTPSERVER.exe          # default port 2700
./build/Debug/HTTPSERVER.exe 8080     # custom port
# Creating server with: 8 threads.
# Server listening on port 8080...
```

Press CTRL-C to shut down gracefully.

## Defining Routes

Routes are registered in `src/UserRoutes.cpp`. Each route takes a method, path, optional middleware list, and a handler that mutates the response in place.

```cpp
void addUserRoutes(RadixTree& router) {

    // Basic route
    router.add("/users", "GET", {requestLogger, makeCors(corsOpts)},
        [](HTTPRequest& req, HTTPResponse& res) {
            res.code = "200"; res.reason = "OK"; res.version = "HTTP/1.1";
            res.headers["Content-Type"] = "application/json";
            res.body = nlohmann::json{{"message", "hello"}}.dump();
        });

    // Named URL parameter - available as req.head.params.at("id")
    router.add("/users/:id", "GET", {requestLogger},
        [](HTTPRequest& req, HTTPResponse& res) {
            res.code = "200"; res.reason = "OK"; res.version = "HTTP/1.1";
            res.body = "user id: " + req.head.params.at("id");
        });

    // Wildcard - captures full subpath including slashes into req.head.params.at("*")
    router.add("/public/*", "GET", {},
        [](HTTPRequest& req, HTTPResponse& res) {
            std::string filePath = "public/" + req.head.params.at("*");
            std::ifstream file(filePath);
            if (!file) {
                res.code = "404"; res.reason = "Not Found"; res.version = "HTTP/1.1";
                return;
            }
            res.body = std::string(std::istreambuf_iterator<char>(file), {});
            res.code = "200"; res.reason = "OK"; res.version = "HTTP/1.1";
            res.headers["Content-Type"] = "text/html";
        });
}
```

### Route priority

When multiple route types could match the same path, the router resolves them in this order:

1. Exact literal match
2. Named parameter (`:id`)
3. Wildcard (`*`)

So `/users/42` always hits `/users/:id` before `/users/*`.

### Query parameters

```cpp
std::string page = req.head.queryParams.count("page")
    ? req.head.queryParams.at("page") : "1";
```

### Middleware

```cpp
MiddleWare authMiddleware = [](HTTPRequest& req, HTTPResponse& res, Next next) {
    if (!req.head.headers.count("Authorization")) {
        res.code = "401"; res.reason = "Unauthorized"; res.version = "HTTP/1.1";
        return; // do not call next() - short-circuits the chain
    }
    next();
};
```

### CORS

```cpp
CorsOptions opts = {
    .allowedOrigins = {"https://example.com"},
    .allowedMethods = {"GET", "POST", "OPTIONS"},
    .allowedHeaders = {"Content-Type", "Authorization"}
};
MiddleWare cors = makeCors(opts);

router.add("/api/data", "GET", {cors}, handler);
router.add("/api/data", "OPTIONS", {cors}, [](HTTPRequest&, HTTPResponse& res) {
    res.code = "204"; res.reason = "No Content"; res.version = "HTTP/1.1";
});
```

Use `allowedOrigins = {"*"}` to permit any origin.

## Platform

Windows only - depends on WinSock2 (`ws2_32`). No external dependencies beyond the Windows SDK and the vendored `json.hpp`.

## Known Limitations

- **Single param or wildcard per node** - a path segment level can have either `:param` or `*`, not both
- **No TLS** - HTTPS would require integrating a library such as OpenSSL or mbedTLS
- **Windows only** - WinSock2 is not portable; a POSIX socket abstraction would be needed for Linux/macOS
