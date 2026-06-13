#pragma once
#include<winsock2.h>
#include <ws2tcpip.h>
#include <stdexcept>
#include <format>

class SocketGuard {
public:
	SocketGuard(SOCKET socket) : _socket(socket) {};
	SocketGuard() : _socket(INVALID_SOCKET) {};

	~SocketGuard() {
		if (_socket != INVALID_SOCKET) {
			closesocket(_socket);
		}
	}

	SocketGuard(SocketGuard&& other) noexcept {
		_socket = other._socket;
		other._socket = INVALID_SOCKET;
	}
	SocketGuard& operator=(SocketGuard&& other) noexcept {
		if (this != &other) {
			if (_socket != INVALID_SOCKET) {
				closesocket(_socket);
			}
			_socket = other._socket;
			other._socket = INVALID_SOCKET;
			return *this;
		}
		return *this;
	}

	SocketGuard(const SocketGuard& other) = delete;
	SocketGuard& operator=(const SocketGuard&) = delete;
	void createSocket(int addresFamily, int adressType, int adressProtocol);
	void bindSocket(const sockaddr* addrName, int addrLength);
	void listenSocket();
	void setTimeout(size_t seconds);
	bool isValid();
	SocketGuard acceptSocket();
	void sendData(const std::string& data);
	int recvData(char* buffer, int size);


private:
	SOCKET _socket;
};