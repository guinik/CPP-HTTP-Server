
#include "SocketGuard.hpp"
void SocketGuard::sendData(const std::string& data) {
	if (_socket == INVALID_SOCKET) {
		throw std::runtime_error("Cannot send on invalid socket");
	}

	int result = send(_socket, data.c_str(), static_cast<int>(data.size()), 0);

	if (result == SOCKET_ERROR) {
		throw std::runtime_error(
			std::format("Send failed: {}", WSAGetLastError())
		);
	}
}

void SocketGuard::createSocket(int addrFamily, int addrType, int addrProtocol) {
	if (_socket != INVALID_SOCKET) {
		closesocket(_socket);
	}
	_socket = socket(addrFamily, addrType, addrProtocol);
	if (_socket == INVALID_SOCKET) {
		throw std::runtime_error(std::format("Invalid Listen Socket. Configuration Might be Failing. : {}", WSAGetLastError()));
	}

}
void SocketGuard::bindSocket(const sockaddr* addrName, int addrLength) {
	if (_socket == INVALID_SOCKET) {
		throw std::runtime_error(std::format("Can not bind an invalid socket, have you created the Socket?"));
	}
	int iResult;
	iResult = bind(_socket, addrName, addrLength);
	if (iResult == SOCKET_ERROR) {
		throw std::runtime_error(std::format("Bind Failed : {}", WSAGetLastError()));
	}

}

void SocketGuard::listenSocket() {
	if (_socket == INVALID_SOCKET) {
		throw std::runtime_error(std::format("Can not bind an invalid socket, have you created the Socket?."));
	}

	if (listen(_socket, SOMAXCONN) == SOCKET_ERROR) {
		throw std::runtime_error(std::format("Listen Failed : {}", WSAGetLastError()));
	}
}


SocketGuard SocketGuard::acceptSocket() {
	if (_socket == INVALID_SOCKET) {
		throw std::runtime_error(std::format("Can not accept an invalid socket, have you created the Socket?"));
	}

	SOCKET clientSocket = accept(_socket, NULL, NULL);
	if (clientSocket == INVALID_SOCKET) {
		throw std::runtime_error(std::format("Accept Failed : {}", WSAGetLastError()));
	}
	return SocketGuard(clientSocket);
}


int SocketGuard::recvData(char* buffer, int size) {
	if (_socket == INVALID_SOCKET) {
		throw std::runtime_error("Cannot recv on invalid socket");
	}

	int bytes = recv(_socket, buffer, size, 0);

	if (bytes == SOCKET_ERROR) {
		
		int err = WSAGetLastError();

		if (err == WSAECONNRESET) {
			return 0; // treat as disconnect, NOT fatal
		}

		throw std::runtime_error(
			std::format("Recv failed: {}", err)
		);

	}

	return bytes;
}