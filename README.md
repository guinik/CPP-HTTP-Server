# C++ HTTP Server

A lightweight HTTP/1.1 server built from scratch in C++20 for Windows, featuring a radix tree router, middleware pipeline, thread pool, CORS, and RAII socket management.

## Features

- **Radix tree router** - O(path depth) route matching with support for named URL parameters (`:id`)
- **HTTP/1.1 parser** - parses raw TCP bytes into structured request objects (method, path, headers, body)
- **Thread pool** - fixed-size pool sized to `hardware_concurrency()`; no unbounded thread spawning
- **Middleware pipeline** - per-route middleware chain with `Next` continuation; built-in `parseJson` and `makeCors` middleware
- **CORS** - configurable allowed origins, methods, and headers; handles `OPTIONS` preflight automatically
- **Query parameter parsing** - `?key=value` pairs extracted into `req.head.queryParams`
- **JSON body parsing** - `Content-Type: application/json` bodies parsed into `req.body.json` (nlohmann/json)
- **Graceful shutdown** - CTRL-C (SIGINT) sets a stop flag; accept loop drains and exits cleanly
- **RAII wrappers** - `SocketGuard`, `AddrInfoGuard`, and `WinSocketGuard` ensure clean resource cleanup on every code path
- **Typed error hierarchy** - distinct exception types for each WSA failure stage

## Architecture

```
main.cpp
  └── registers routes via addUserRoutes()
  └── creates ThreadPool (hardware_concurrency threads)
  └── creates WSAHandler → calls run()

WSAHandler::run()
  └── sets up TCP listen socket (1 s accept timeout for shutdown polling)
  └── accept loop → enqueues connection on ThreadPool
  └── checks g_running flag; exits loop on SIGINT

HandleConnection()
  └── reads raw bytes from socket
  └── parseRawBytesHeadRequest() + parseRawBytesBodyRequest() → HTTPRequest
  └── RadixTree::match() → finds Route (populates req.head.params + queryParams)
  └── applyRoute() → runs middleware chain → calls handler → HTTPResponse
  └── HTTPResponseToRawString() → sends back over socket
```

## Project Structure

```
include/
  AddrInfo.hpp          - RAII wrapper for getaddrinfo result
  SocketGuard.hpp       - RAII wrapper for SOCKET handle
  WSA.hpp               - WSAHandler class + WinSocketGuard (WSAStartup/Cleanup)
  HTTPRequest.hpp       - HTTPRequest/HTTPHead/HTTPBody structs + parser declarations
  HTTPResponse.hpp      - HTTPResponse struct
  Router.hpp            - RadixTree, RadixTreeNode, Route, MiddleWare, Handler types
  HandleConnection.hpp  - HandleConnection() + helpers
  Cors.hpp              - makeCors() middleware factory
  Utils.hpp             - parseJson middleware
  UserRoutes.hpp        - addUserRoutes() declaration
  json.hpp              - nlohmann/json (single-header, vendored)

src/
  WSA.cpp               - WSAHandler::run() (listen loop, thread pool dispatch)
  HandleConnection.cpp  - connection lifecycle + response serializer
  HTTPRequest.cpp       - raw bytes → HTTPRequest parser (head, body, query params)
  Router.cpp            - RadixTree::add(), RadixTree::match(), applyRoute()
  UserRoutes.cpp        - user-defined route handlers
  ThreadPool.cpp        - fixed-size thread pool
  main.cpp              - entry point, signal handler, wires up components
```

## Building

**Requirements:** Windows, CMake 3.20+, MSVC (C++20)

```bash
cmake -S . -B build
cmake --build build
```

## Running

```bash
./build/Debug/HTTPSERVER.exe
# Creating server with: 8 threads.
# Server listening on port 2700...
```

Press CTRL-C to shut down gracefully.

## Defining Routes

Add routes in `src/UserRoutes.cpp`. Handlers receive the request and mutate the response in place. Each route takes an optional middleware list that runs before the handler.

```cpp
void addUserRoutes(RadixTree& router) {
    // Route with middleware
    router.add("/users", "GET", {parseJson, userCorsMiddleWare},
        [](const HTTPRequest& req, HTTPResponse& res) {
            res.code = "200";
            res.reason = "OK";
            res.headers["Content-Type"] = "application/json";
            res.body = nlohmann::json{{"message", "hello"}}.dump();
        });

    // Named URL parameter
    router.add("/users/:id", "GET", {userCorsMiddleWare},
        [](const HTTPRequest& req, HTTPResponse& res) {
            res.code = "200";
            res.reason = "OK";
            res.body = "user id: " + req.head.params.at("id");
        });
}
```

### Query Parameters

Available in `req.head.queryParams`:

```cpp
std::string page = req.head.queryParams.count("page")
    ? req.head.queryParams.at("page") : "1";
```

### Middleware

Middleware functions have the signature `void(HTTPRequest&, HTTPResponse&, Next)`. Call `next()` to continue the chain or return early to short-circuit.

```cpp
MiddleWare myMiddleware = [](HTTPRequest& req, HTTPResponse& res, Next next) {
    if (!req.head.headers.count("Authorization")) {
        res.code = "401";
        res.reason = "Unauthorized";
        res.version = "HTTP/1.1";
        return;
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
// Also register an OPTIONS route for preflight:
router.add("/api/data", "OPTIONS", {cors}, [](const HTTPRequest&, HTTPResponse& res) {
    res.code = "204"; res.reason = "No Content"; res.version = "HTTP/1.1";
});
```

Use `allowedOrigins = {"*"}` to permit any origin.

## Example

```bash
curl http://localhost:2700/users
# {"limit":"not set","message":"No message","page":"not set"}

curl "http://localhost:2700/users?page=2&limit=10"
# {"limit":"10","message":"No message","page":"2"}

curl http://localhost:2700/users/42
# user id is: 42

curl http://localhost:2700/unknown
# HTTP/1.1 404 Path not found
```

## Platform

Windows only - uses WinSock2 (`ws2_32`). No external dependencies beyond the Windows SDK and the vendored `json.hpp`.

## Known Limitations

This is a learning project - the following are known gaps, not bugs to fix right now:

- **No keep-alive** - each connection is handled once and closed. HTTP/1.1 persistent connections are not supported.
- **Single param child per node** - the radix tree supports one named parameter per path segment level; deeper nested params (e.g. `/a/:x/b/:y`) may need testing.
- **Windows only** - depends on WinSock2; not portable to Linux/macOS without replacing the networking layer.
