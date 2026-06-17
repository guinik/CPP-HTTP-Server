#include "SocketGuard.hpp"
#include <iostream>

#ifdef _WIN32
    #define SOCKET_ERRNO WSAGetLastError()
    #define SOCK_CLOSE(s) closesocket(s)
    #define SOCK_ERROR SOCKET_ERROR
#else
    #define SOCKET_ERRNO errno
    #define SOCK_CLOSE(s) close(s)
    #define SOCK_ERROR -1
#endif

void SocketGuard::send(const std::string& data) {
	if (_socket == INVALID_HANDLE) {
		throw std::runtime_error("Cannot send on invalid socket");
	}

	size_t totalSent{ 0 };
	while (totalSent < data.size()) {
		int result = ::send(_socket, data.c_str() + totalSent, static_cast<int>(data.size() - totalSent), 0);

		if (result == SOCK_ERROR) {
			int err = SOCKET_ERRNO;
#ifdef _WIN32
			if (err == WSAECONNRESET || err == WSAECONNABORTED) return;
#else
			if (err == ECONNRESET || err == EPIPE) return;
#endif
			throw std::runtime_error(std::format("Send failed: {}", err));
		}

		totalSent += static_cast<size_t>(result);
	}
}

void SocketGuard::create(int addrFamily, int addrType, int addrProtocol) {
	if (_socket != INVALID_HANDLE) {
		SOCK_CLOSE(_socket);
	}
	_socket = socket(addrFamily, addrType, addrProtocol);
	if (_socket == INVALID_HANDLE) {
		throw std::runtime_error(std::format("Invalid Listen Socket: {}", SOCKET_ERRNO));
	}
	int opt = 1;
	setsockopt(_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
}

void SocketGuard::bind(const sockaddr* addrName, int addrLength) {
	if (_socket == INVALID_HANDLE) {
		throw std::runtime_error("Cannot bind an invalid socket");
	}
	if (::bind(_socket, addrName, addrLength) == SOCK_ERROR) {
		throw std::runtime_error(std::format("Bind failed: {}", SOCKET_ERRNO));
	}
}

void SocketGuard::listen() {
	if (_socket == INVALID_HANDLE) {
		throw std::runtime_error("Cannot listen on an invalid socket");
	}
	if (::listen(_socket, SOMAXCONN) == SOCK_ERROR) {
		throw std::runtime_error(std::format("Listen failed: {}", SOCKET_ERRNO));
	}
}

void SocketGuard::setTimeout(size_t seconds) {
#ifdef _WIN32
	DWORD msTime = static_cast<DWORD>(seconds * 1000);
	if (setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, (const char*)&msTime, sizeof(msTime)) == SOCK_ERROR) {
		throw std::runtime_error(std::format("Set timeout failed: {}", SOCKET_ERRNO));
	}
#else
	struct timeval tv{};
	tv.tv_sec = static_cast<long>(seconds);
	tv.tv_usec = 0;
	if (setsockopt(_socket, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == SOCK_ERROR) {
		throw std::runtime_error(std::format("Set timeout failed: {}", SOCKET_ERRNO));
	}
#endif
}

SocketGuard SocketGuard::accept() {
	if (_socket == INVALID_HANDLE) {
		throw std::runtime_error("Cannot accept on an invalid socket");
	}

	fd_set readSet;
	FD_ZERO(&readSet);
	FD_SET(_socket, &readSet);

	timeval timeout{};
	timeout.tv_sec = 1;
	timeout.tv_usec = 0;

	int ready = select(static_cast<int>(_socket + 1), &readSet, NULL, NULL, &timeout);

	if (ready == 0) return SocketGuard(INVALID_HANDLE);
	if (ready == SOCK_ERROR) throw std::runtime_error("select() failed");

	SocketHandle clientSocket = ::accept(_socket, NULL, NULL);
	if (clientSocket == INVALID_HANDLE) {
		throw std::runtime_error(std::format("Accept failed: {}", SOCKET_ERRNO));
	}
	return SocketGuard(clientSocket);
}

int SocketGuard::recv(char* buffer, int size) {
	if (_socket == INVALID_HANDLE) {
		throw std::runtime_error("Cannot recv on invalid socket");
	}

	int bytes = ::recv(_socket, buffer, size, 0);

	if (bytes == SOCK_ERROR) {
		int err = SOCKET_ERRNO;
#ifdef _WIN32
		if (err == WSAECONNRESET || err == WSAECONNABORTED || err == WSAESHUTDOWN || err == WSAETIMEDOUT)
#else
		if (err == ECONNRESET || err == ENOTCONN || err == ETIMEDOUT || err == EAGAIN)
#endif
			throw SocketDisconnectException(std::format("Client disconnected: {}", err));

		throw SocketBaseError(std::format("Recv failed: {}", err));
	}

	if (bytes == 0) {
		throw SocketDisconnectException("Client disconnected: connection closed");
	}

	return bytes;
}

bool SocketGuard::isValid() const {
	return _socket != INVALID_HANDLE;
}

std::string SocketGuard::peerAddress() const {
	if (_socket == INVALID_HANDLE) return "unknown";
	sockaddr_storage addr{};
#ifdef _WIN32
	int addrLen = sizeof(addr);
#else
	socklen_t addrLen = sizeof(addr);
#endif
	if (::getpeername(_socket, reinterpret_cast<sockaddr*>(&addr), &addrLen) != 0)
		return "unknown";
	char host[NI_MAXHOST]{};
	char svc[NI_MAXSERV]{};
	if (::getnameinfo(reinterpret_cast<const sockaddr*>(&addr),
	                  static_cast<socklen_t>(addrLen),
	                  host, NI_MAXHOST, svc, NI_MAXSERV,
	                  NI_NUMERICHOST | NI_NUMERICSERV) != 0)
		return "unknown";
	return std::string(host) + ":" + svc;
}
