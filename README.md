# C++ HTTP Server

A lightweight HTTP/1.1 server built from scratch in C++20 for Windows, featuring a radix tree router, RAII socket management, and per-connection multithreading.

## Features

- **Radix tree router** — O(path depth) route matching with support for named URL parameters (`:id`)
- **HTTP/1.1 parser** — parses raw TCP bytes into structured request objects (method, path, headers, body)
- **Multithreaded** — each incoming connection is handled on a detached thread
- **RAII wrappers** — `SocketGuard`, `AddrInfoGuard`, and `WinSocketGuard` ensure clean resource cleanup on every code path
- **Typed error hierarchy** — distinct exception types for each WSA failure stage

## Architecture

```
main.cpp
  └── registers routes via addUserRoutes()
  └── creates WSAHandler → calls run()

WSAHandler::run()
  └── sets up TCP listen socket
  └── accept loop → spawns thread per connection

HandleConnection()
  └── reads raw bytes from socket
  └── parseRawBytesRequest() → HTTPRequest
  └── RadixTree::match() → finds handler (populates params)
  └── calls handler → HTTPResponse
  └── HTTPResponseToRawString() → sends back over socket
```

## Project Structure

```
include/
  AddrInfo.hpp        — RAII wrapper for getaddrinfo result
  SocketGuard.hpp     — RAII wrapper for SOCKET handle
  WSA.hpp             — WSAHandler class + WinSocketGuard (WSAStartup/Cleanup)
  HTTPRequest.hpp     — HTTPRequest struct + parser declaration
  HTTPResponse.hpp    — HTTPResponse struct
  Router.hpp          — RadixTree and RadixTreeNode definitions
  HandleConnection.hpp
  UserRoutes.hpp

src/
  WSA.cpp             — WSAHandler::run() (listen loop, thread dispatch)
  HandleConnection.cpp — connection lifecycle + response serializer
  HTTPRequest.cpp     — raw bytes → HTTPRequest parser
  Router.cpp          — RadixTree::add() and RadixTree::match()
  UserRoutes.cpp      — user-defined route handlers
  main.cpp            — entry point
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
# Server listening on port 2700...
```

## Defining Routes

Add routes in `src/UserRoutes.cpp`:

```cpp
void addUserRoutes(RadixTree& router) {
    router.add("/users", "GET", [](const HTTPRequest& req) -> HTTPResponse {
        return { .version = "HTTP/1.1", .code = "200", .reason = "OK", .body = "hello" };
    });

    router.add("/users/:id", "GET", [](const HTTPRequest& req) -> HTTPResponse {
        return { .version = "HTTP/1.1", .code = "200", .reason = "OK",
                 .body = "user id: " + req.params.at("id") };
    });
}
```

Named parameters (`:name`) are automatically extracted from the URL and available via `req.params`.

## Example

```bash
curl http://localhost:2700/users
# hellooooo

curl http://localhost:2700/users/42
# user id is: 42

curl http://localhost:2700/unknown
# HTTP/1.1 404 Path not found
```

## Platform

Windows only — uses WinSock2 (`ws2_32`). No external dependencies beyond the Windows SDK.

## Known Limitations

This is a learning project — the following are known gaps, not bugs to fix right now:

- **No `Content-Length` handling** — the recv loop stops at `\r\n\r\n` (end of headers) and does not read the body based on `Content-Length`. POST requests with large bodies will be truncated.
- **No keep-alive** — each connection is handled once and closed. HTTP/1.1 persistent connections are not supported.
- **Detached threads with no limit** — each connection spawns a detached `std::thread` with no thread pool or cap. Under load this will exhaust system resources.
- **Single param child per node** — the radix tree only supports one named parameter per path segment level (e.g. `/users/:id` works, but `/users/:id/posts/:postId` would need testing).
- **No query string parsing** — `?key=value` parameters in the URL are not parsed and will be treated as part of the path.
- **Windows only** — depends on WinSock2; not portable to Linux/macOS without replacing the networking layer.
