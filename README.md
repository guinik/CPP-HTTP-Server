# C++ HTTP Server

A lightweight HTTP/1.1 server built from scratch in C++20. No frameworks, no dependencies beyond a vendored JSON header.

## Features

- **Trie router** - O(path depth) matching with named parameters (`:id`), wildcard segments (`*`), and DFS backtracking for correct specificity
- **HTTP/1.1 keep-alive** - persistent connections reused across multiple requests; respects `Connection: close` and HTTP/1.0 defaults
- **Thread pool** - fixed-size pool sized to `hardware_concurrency()`; no unbounded thread spawning
- **Middleware pipeline** - per-route chain with `Next` continuation; short-circuit by not calling `next()`; handler runs inside the chain so middleware captures accurate timing and response codes
- **Static file serving** - wildcard routes serve files from disk with MIME type detection and path traversal protection
- **CORS** - configurable origins, methods, and headers; handles `OPTIONS` preflight automatically
- **JSON body parsing** - `Content-Type: application/json` bodies parsed automatically into `req.body.json` (nlohmann/json)
- **Query parameter parsing** - `?key=value` pairs URL-decoded into `req.head.queryParams`
- **Case-insensitive headers** - header lookup follows RFC 7230; `Content-Type` and `content-type` are the same key
- **Request logging** - built-in middleware logs method, path, status, and latency
- **Graceful shutdown** - CTRL-C sets a stop flag; accept loop drains and exits cleanly
- **RAII everywhere** - `SocketGuard`, `AddrInfoGuard`, and `WinSocketGuard` ensure clean resource cleanup on every code path
- **Cross-platform** - runs on Windows (Winsock2) and Linux (POSIX sockets); same codebase, `#ifdef` at the socket layer only
- **Docker + Nginx** - ships with a `Dockerfile`, `docker-compose.yml`, and nginx config for containerized HTTPS deployment

## Architecture

```
main.cpp
  └── parses port from argv (default 2700)
  └── registers routes via addUserRoutes()
  └── creates ThreadPool (hardware_concurrency threads)
  └── creates HttpServer → calls run()

HttpServer::run()
  └── sets up TCP listen socket
  └── accept loop → enqueues HandleConnection on ThreadPool
  └── polls g_running flag; exits on SIGINT

HandleConnection()  [runs on thread pool]
  └── keep-alive loop:
        └── ReadRequestHead() → ReadRequestBody() → HTTPRequest
        └── RouteTrie::match() → node (populates req.head.params + queryParams)
        └── method lookup → 404 / 405 / applyRoute()
        └── applyRoute() → middleware chain → handler → HTTPResponse
        └── HTTPResponseToRawString() → send()
        └── break if Connection: close
```

## Project Structure

```
include/
  AddrInfo.hpp       - RAII wrapper for getaddrinfo result
  SocketGuard.hpp    - RAII wrapper for socket handle (cross-platform)
  HttpServer.hpp     - HttpServer class + WinSocketGuard (Windows-only guard)
  HTTPRequest.hpp    - HTTPRequest / HTTPHead / HTTPBody structs + parser declarations
  HTTPResponse.hpp   - HTTPResponse struct
  Router.hpp         - RouteTrie, RouteTrieNode, Route, MiddleWare, Handler types
  HandleConnection.hpp
  Cors.hpp           - makeCors() middleware factory
  Utils.hpp          - parseJson + requestLogger middleware
  UserRoutes.hpp     - addUserRoutes() declaration
  json.hpp           - nlohmann/json (single-header, vendored)

src/
  main.cpp              - entry point, port config, signal handler
  HttpServer.cpp        - listen loop, thread pool dispatch
  HandleConnection.cpp  - connection lifecycle, keep-alive loop, response serializer
  HTTPRequest.cpp       - raw bytes → HTTPRequest (head, body, query params)
  Router.cpp            - RouteTrie::add(), match(), dfsFindMatch(), applyRoute()
  UserRoutes.cpp        - user-defined route handlers
  ThreadPool.cpp        - fixed-size thread pool
  SocketGuard.cpp       - send, recv, bind, listen, accept wrappers

tests/
  test_parser.cpp     - unit tests for HTTP request parsing and splitByDelimiter
  test_router.cpp     - unit tests for URL decoding, radix tree matching, and applyRoute
  test_middleware.cpp - unit tests for CORS and parseJson middleware

nginx/
  nginx.conf          - reverse proxy config (HTTPS termination → HTTP to app)
```

## Building

### Windows

**Requirements:** CMake 3.20+, MSVC (C++20)

```bash
cmake -S . -B build
cmake --build build
```

### Linux

**Requirements:** CMake 3.20+, GCC 14+ or Clang with C++20 support

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Docker

```bash
docker-compose up --build
```

Builds the server in a Linux container and starts nginx in front of it. HTTPS available at `https://localhost`.

**Place your SSL certificate and key at:**
```
nginx/server.crt
nginx/server.key
```

## Running

```bash
./build/Debug/http_server          # default port 2700
./build/Debug/http_server 8080     # custom port
# Creating server with: 8 threads.
# Server listening on port 8080...
```

Press CTRL-C to shut down gracefully.

## Testing

Tests use [GoogleTest](https://github.com/google/googletest), fetched automatically by CMake.

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

| Suite | What it tests |
|---|---|
| `SplitByDelimiter`, `ParseHead`, `ParseBody` | HTTP request parser — raw bytes to struct |
| `StringDecode`, `Router`, `ApplyRoute` | URL decoding, radix tree matching, middleware chain |
| `Cors`, `ParseJsonMiddleware` | CORS origin validation and JSON body parsing |

CI runs on every pull request via GitHub Actions on both **Windows** and **Ubuntu**.

## Defining Routes

Routes are registered in `src/UserRoutes.cpp`.

```cpp
void addUserRoutes(RouteTrie& router) {

    // Basic route
    router.add("/users", "GET", {requestLogger, makeCors(corsOpts)},
        [](const HTTPRequest& req, HTTPResponse& res) {
            res.code = "200"; res.reason = "OK"; res.version = "HTTP/1.1";
            res.headers["Content-Type"] = "application/json";
            res.body = nlohmann::json{{"message", "hello"}}.dump();
        });

    // Named URL parameter — available as req.head.params.at("id")
    router.add("/users/:id", "GET", {requestLogger},
        [](const HTTPRequest& req, HTTPResponse& res) {
            res.code = "200"; res.reason = "OK"; res.version = "HTTP/1.1";
            res.body = "user id: " + req.head.params.at("id");
        });

    // Wildcard — captures full subpath into req.head.params.at("*")
    // Path traversal is blocked: resolved path must stay within /public
    router.add("/public/*", "GET", {},
        [](const HTTPRequest& req, HTTPResponse& res) {
            // ... filesystem::canonical check then ifstream
        });
}
```

### Route priority

1. Exact literal match
2. Named parameter (`:id`)
3. Wildcard (`*`)

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
        return; // do not call next() — short-circuits the chain
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
router.add("/api/data", "OPTIONS", {cors}, [](const HTTPRequest&, HTTPResponse& res) {
    res.code = "204"; res.reason = "No Content"; res.version = "HTTP/1.1";
});
```

Use `allowedOrigins = {"*"}` to permit any origin.

## HTTPS / Nginx

TLS is handled by nginx as a reverse proxy. The included `nginx/nginx.conf` terminates SSL on port 443 and forwards plain HTTP to the server on port 2700.

```
Client (HTTPS:443) → Nginx → (HTTP:2700) → C++ server
```

With Docker Compose this is fully automated:

```bash
docker-compose up --build
```

For standalone nginx, adapt `nginx/nginx.conf` to your cert paths and run:

```nginx
location / {
    proxy_pass http://127.0.0.1:2700;
    proxy_set_header Host              $host;
    proxy_set_header X-Real-IP         $remote_addr;
    proxy_set_header X-Forwarded-For   $proxy_add_x_forwarded_for;
    proxy_set_header X-Forwarded-Proto $scheme;
}
```

## Known Limitations

- **Single param or wildcard per node** - a path segment level can have either `:param` or `*`, not both
- **No TLS** - HTTPS is delegated to nginx (see above)
