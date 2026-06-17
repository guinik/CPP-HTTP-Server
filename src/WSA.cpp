#include "WSA.hpp"
#include "Logger.hpp"
#include <cstring>
#include <format>


void HttpServer::run() {
	struct addrinfo hints;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	hints.ai_flags = AI_PASSIVE;

	AddrInfoGuard SecureAddrInfo = AddrInfoGuard(NULL, _PORT.c_str(), &hints);

	const addrinfo* info = SecureAddrInfo.get();

	// Need to cleann addr info on result
	SocketGuard SafeListenSocket = SocketGuard();
	SafeListenSocket.createSocket(info->ai_family, info->ai_socktype, info->ai_protocol);
	SafeListenSocket.bindSocket(info->ai_addr, static_cast<int>(info->ai_addrlen));
	SafeListenSocket.listenSocket();
	Log::info(std::format("Server listening on port {}...", _PORT));
	while (_running) {
		SocketGuard client = SafeListenSocket.acceptSocket();
		if(client.isValid())
		{
			if (_activeConnections.load(std::memory_order_relaxed) >= kMaxConnections) {
				// SocketGuard destructor closes the socket → client gets a RST.
				// The OS SYN backlog then provides natural back-pressure.
				continue;
			}
			++_activeConnections;
			_threadPool.enqueue(
				[c = std::move(client), this]() mutable {
					HandleConnection(std::move(c), _router, _running);
					--_activeConnections;
				}
			);
		}
	}
	Log::info("Closed server.");

}
