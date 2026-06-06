#include "WSA.hpp"
#include <iostream>

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

		client.sendData("Hello!\n");

		char buffer[512];

		while (true) {
			int bytes = client.recvData(buffer, sizeof(buffer));

			if (bytes <= 0) break;

			buffer[bytes] = '\0';

			std::cout << "Client: " << buffer << std::endl;

			client.sendData(buffer); // echo
		}
	}

}

