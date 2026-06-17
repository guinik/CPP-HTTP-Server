# C++ HTTP Server

A lightweight HTTP/1.1 server built from scratch in C++20. No frameworks, no dependencies beyond a vendored JSON header.

## Features

- **Trie router** - O(path depth) matching with named parameters (`:id`), wildcard segments (`*`), and DFS backtracking for correct specificity
- **HTTP/1.1 keep-alive** - persistent connections reused across multiple requests; respects `Connection: close` and HTTP/1.0 defaults
- **Thread pool** - fixed-size pool sized to `hardware_concurrency()`; bounded task queue (default 4096) rejects excess connections with a 503 rather than growing without limit; `enqueue` throws on shutdown so the accept loop can't leak counters
- **Middleware pipeline** - per-route chain with `Next` continuation; short-circuit by not calling `next()`; handler runs inside the chain so middleware captures accurate timing and response codes
- **Static file serving** - wildcard routes serve files from disk with MIME type detection and path traversal protection
- **CORS** - configurable origins, methods, and headers; handles `OPTIONS` preflight automatically
- **JSON body parsing** - `Content-Type: application/json` bodies parsed automatically into `req.body.json` (nlohmann/json)
- **Query parameter parsing** - `?key=value` pairs URL-decoded into `req.head.queryParams`
- **Case-insensitive headers** - header lookup follows RFC 7230; `Content-Type` and `content-type` are the same key
- **Request logging** - built-in middleware logs method, path, status, and latency
- **Graceful shutdown** - CTRL-C sets a stop flag; accept loop drains and exits cleanly; `activeConnections` is decremented via a try/finally guard so the drain counter is always accurate even when a handler throws
- **RAII everywhere** - `SocketGuard`, `AddrInfoGuard`, and `WinSocketGuard` ensure clean resource cleanup on every code path
- **Prometheus metrics** - `GET /metrics/prometheus` serves the standard text exposition format (counters, gauge, and a request-latency histogram with 11 fixed buckets) — scrapable by Prometheus, Grafana Agent, or any OpenMetrics-compatible collector; `GET /metrics` continues to serve the JSON snapshot
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
  Metrics.hpp        - atomic counters + latency histogram; JSON and Prometheus snapshots
  ServerConfig.hpp   - all server tunables with defaults; overridable via env vars
  json.hpp           - nlohmann/json (single-header, vendored)

src/
  main.cpp              - entry point, port/env config, signal handler
  HttpServer.cpp        - listen loop, thread pool dispatch, connection drain on shutdown
  HandleConnection.cpp  - connection lifecycle, keep-alive loop, per-request latency recording
  HTTPRequest.cpp       - raw bytes → HTTPRequest (head, body, query params)
  Router.cpp            - RouteTrie::add(), match(), dfsFindMatch(), applyRoute()
  UserRoutes.cpp        - user-defined route handlers + /metrics/prometheus endpoint
  ThreadPool.cpp        - fixed-size pool with bounded queue
  SocketGuard.cpp       - send, recv, bind, listen, accept wrappers

tests/
  test_parser.cpp      - unit tests for HTTP request parsing and splitByDelimiter
  test_router.cpp      - unit tests for URL decoding, trie matching, and applyRoute
  test_middleware.cpp  - unit tests for CORS and parseJson middleware
  test_threadpool.cpp  - concurrency, exception safety, bounded queue, enqueue-after-stop
  test_serializer.cpp  - response serialization and CRLF injection prevention
  test_connection.cpp  - integration tests over real TCP loopback sockets (keep-alive,
                         chunked TE, Slowloris, static files, path traversal)

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
```

Press CTRL-C to shut down gracefully. The accept loop stops, waits up to `shutdownTimeout` seconds for in-flight connections to finish, then exits.

### Environment variables

All tunables can be overridden at startup without recompiling:

| Variable | Default | Description |
|---|---|---|
| `HTTP_PORT` | `2700` | TCP port to bind |
| `HTTP_BIND` | `0.0.0.0` | Bind address |
| `HTTP_THREADS` | `hardware_concurrency` | Worker thread count |
| `HTTP_MAX_CONN` | `1000` | Max simultaneous connections |
| `HTTP_QUEUE_DEPTH` | `4096` | Max pending tasks in the thread pool queue; excess connections are rejected with 503 |
| `HTTP_LOG_LEVEL` | `info` | One of `debug`, `info`, `warn`, `error` |

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
| `StringDecode`, `Router`, `ApplyRoute` | URL decoding, trie matching, middleware chain |
| `Cors`, `ParseJsonMiddleware` | CORS origin validation and JSON body parsing |
| `ThreadPool` | Concurrency, exception safety, bounded queue rejection, enqueue-after-stop |
| `Serializer` | Response format, `Content-Length`, CRLF injection prevention |
| `ConnectionTest` | Full round-trips over real TCP loopback: keep-alive cycling, HTTP/1.0, chunked TE, Slowloris timeout, per-connection request limit, pipelining |
| `StaticFileTest` | File serving, 404, path traversal rejection, oversized file rejection |

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

## Observability

### Endpoints

| Endpoint | Format | Description |
|---|---|---|
| `GET /health` | JSON | `{"status":"ok"}` — liveness check |
| `GET /metrics` | JSON | Snapshot of all counters |
| `GET /metrics/prometheus` | Prometheus text | Scrapable by Prometheus / Grafana Agent |

### Prometheus scrape config

```yaml
scrape_configs:
  - job_name: cpp_http_server
    static_configs:
      - targets: ["localhost:2700"]
    metrics_path: /metrics/prometheus
```

### Exposed metrics

| Name | Type | Description |
|---|---|---|
| `http_requests_total` | counter | All requests received |
| `http_responses_total{status="2xx\|4xx\|5xx"}` | counter | Responses by status class |
| `http_active_connections` | gauge | Connections currently being handled |
| `http_request_duration_ms` | histogram | End-to-end request latency in ms; buckets: 1, 5, 10, 25, 50, 100, 250, 500, 1000, 2500, 5000, +Inf |

Logs are structured JSON emitted to stdout (`info`, `warn`) and stderr (`error`), one object per line.

## Known Limitations

- **Single param or wildcard per node** - a path segment level can have either `:param` or `*`, not both
- **No TLS** - HTTPS is delegated to nginx (see above)
