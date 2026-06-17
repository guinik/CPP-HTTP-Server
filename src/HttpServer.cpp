#include "HttpServer.hpp"
#include "Logger.hpp"
#include <cstring>
#include <format>
#include <thread>

void HttpServer::run() {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags    = AI_PASSIVE;

    AddrInfoGuard SecureAddrInfo(NULL, _PORT.c_str(), &hints);
    const addrinfo* info = SecureAddrInfo.get();

    SocketGuard SafeListenSocket;
    SafeListenSocket.create(info->ai_family, info->ai_socktype, info->ai_protocol);
    SafeListenSocket.bind(info->ai_addr, static_cast<int>(info->ai_addrlen));
    SafeListenSocket.listen();
    Log::info(std::format("Server listening on port {}...", _PORT));

    while (_running) {
        SocketGuard client = SafeListenSocket.accept();
        if (!client.isValid()) continue;

        // Atomically claim a slot: increment first, then check.
        // This avoids the check-then-increment race where two threads both
        // read N-1 and both pass the limit check before either increments.
        if (++_activeConnections > kMaxConnections) {
            --_activeConnections;
            // SocketGuard destructor closes the socket → client gets a RST.
            // The OS SYN backlog then provides natural back-pressure.
            continue;
        }

        _threadPool.enqueue(
            [c = std::move(client), this]() mutable {
                HandleConnection(std::move(c), _router, _running);
                --_activeConnections;
            }
        );
    }

    // Drain active connections up to the configured timeout so in-flight
    // requests finish cleanly before the thread pool is torn down.
    Log::info("Accept loop stopped; waiting for active connections to drain...");
    auto deadline = std::chrono::steady_clock::now() + _shutdownTimeout;
    while (_activeConnections.load() > 0 &&
           std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    if (_activeConnections.load() > 0) {
        Log::error(std::format("{} connection(s) still active after {}s shutdown timeout.",
                   _activeConnections.load(), _shutdownTimeout.count()));
    }

    Log::info("Server closed.");
}
