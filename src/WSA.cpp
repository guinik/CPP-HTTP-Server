#include "WSA.hpp"
#include <iostream>
#include <thread>

void WSAHandler::run() {
	struct addrinfo hints;
	ZeroMemory(&hints, sizeof(hints));
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
	printf("Server listening on port %s...\n", _PORT.c_str());
	while (true) {
		SocketGuard client = SafeListenSocket.acceptSocket();
		client.setTimeout(5);

		// CLIENT has reached the port TCP connection ready lets pass it to detached void
		_threadPool.enqueue(
			[c = std::make_shared<SocketGuard>(std::move(client)), this]() mutable {
				HandleConnection(std::move(*c), _router);
			}
		);
		

	}

}