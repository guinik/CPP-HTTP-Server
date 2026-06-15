
#include "SocketGuard.hpp"
#include <iostream>
void SocketGuard::sendData(const std::string& data) {
	if (_socket == INVALID_SOCKET) {
		throw std::runtime_error("Cannot send on invalid socket");
	}


	size_t totalSent{ 0 };

	while ( totalSent < data.size())
	{
		int result = send(_socket, data.c_str() + totalSent, static_cast<int>(data.size() - totalSent), 0);

		if (result == SOCKET_ERROR) {

			int err = WSAGetLastError();
			if (err == WSAECONNRESET || err == WSAECONNABORTED) {
				return; // client disconnected mid-send, not fatal
			}

			throw std::runtime_error(
				std::format("Send failed: {}", WSAGetLastError())
			);
		}


		totalSent += result;
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

void SocketGuard::setTimeout(size_t seconds)
{
	DWORD msTime = static_cast<DWORD>(seconds * 1000);
	int iResult;
	iResult = setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&msTime, sizeof(msTime) );
	if (iResult == SOCKET_ERROR) {
		throw std::runtime_error(std::format("Set Timeout Failed : {}", WSAGetLastError()));
	}
}


SocketGuard SocketGuard::acceptSocket() {
	if (_socket == INVALID_SOCKET) {
		throw std::runtime_error(std::format("Can not accept an invalid socket, have you created the Socket?"));
	}

	fd_set readSet;
	FD_ZERO(&readSet);
	FD_SET(_socket, &readSet);

	timeval timeout{};
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	int ready = select(0, &readSet, NULL, NULL, &timeout);

	if (ready == 0)
	{
		return SocketGuard(INVALID_SOCKET);
	}
	if (ready == SOCKET_ERROR)
	{
		throw std::runtime_error("SELECT FAILED");
	}

	SOCKET clientSocket = accept(_socket, NULL, NULL);

	if (clientSocket == INVALID_SOCKET) {
		int error = WSAGetLastError();
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
		if (err == WSAECONNRESET || err == WSAECONNABORTED || err == WSAESHUTDOWN || err == WSAETIMEDOUT) {
			throw SocketDisconnectException(std::format("Client disconnected: {}", err));
		}

		throw SocketBaseError(
			std::format("Recv failed: {}", err)
		);

	}

	if(bytes  == 0)
	{
		throw SocketDisconnectException(std::format("Client disconnected: No bytes recieved"));
	}

	return bytes;
}

bool SocketGuard::isValid() 
{
	return _socket != INVALID_SOCKET;
};