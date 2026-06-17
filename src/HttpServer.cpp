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

    // A bindAddress of "0.0.0.0" maps to AI_PASSIVE behaviour (all interfaces).
    // Any other value binds to that specific address only.
    const char* node = (_config.bindAddress == "0.0.0.0") ? nullptr
                                                           : _config.bindAddress.c_str();

    AddrInfoGuard SecureAddrInfo(node, _config.port.c_str(), &hints);
    const addrinfo* info = SecureAddrInfo.get();

    SocketGuard SafeListenSocket;
    SafeListenSocket.create(info->ai_family, info->ai_socktype, info->ai_protocol);
    SafeListenSocket.bind(info->ai_addr, static_cast<int>(info->ai_addrlen));
    SafeListenSocket.listen();
    Log::info(std::format("Server listening on {}:{}...", _config.bindAddress, _config.port));

    while (_running) {
        SocketGuard client = SafeListenSocket.accept();
        if (!client.isValid()) continue;

        if (++_metrics.activeConnections > _config.maxConnections) {
            --_metrics.activeConnections;
            continue;
        }

        try {
            _threadPool.enqueue(
                [c = std::move(client), this]() mutable {
                    // Decrement activeConnections on every exit path, including
                    // exceptions thrown by HandleConnection (e.g. send failure).
                    try {
                        HandleConnection(std::move(c), _router, _running, _metrics, _config);
                    } catch (const std::exception& e) {
                        Log::error(std::format("[HandleConnection] threw: {}", e.what()));
                    } catch (...) {
                        Log::error("[HandleConnection] threw unknown exception");
                    }
                    { std::lock_guard lk(_drainMtx); --_metrics.activeConnections; }
                    _drainCv.notify_one();
                }
            );
        } catch (const std::runtime_error& e) {
            // Queue full or pool stopping — close the socket and release the slot.
            --_metrics.activeConnections;
            Log::warn(std::format("Connection rejected: {}", e.what()));
        }
    }

    Log::info("Accept loop stopped; waiting for active connections to drain...");
    auto deadline = std::chrono::steady_clock::now() + _config.shutdownTimeout;
    {
        std::unique_lock lk(_drainMtx);
        _drainCv.wait_until(lk, deadline,
            [this] { return _metrics.activeConnections.load() == 0; });
    }
    if (_metrics.activeConnections.load() > 0) {
        Log::error(std::format("{} connection(s) still active after {}s shutdown timeout.",
                   _metrics.activeConnections.load(), _config.shutdownTimeout.count()));
    }

    Log::info("Server closed.");
}
