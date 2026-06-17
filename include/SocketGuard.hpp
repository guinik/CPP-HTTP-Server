#pragma once
#include <stdexcept>
#include <format>

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
    using SocketHandle = SOCKET;
    constexpr SocketHandle INVALID_HANDLE = INVALID_SOCKET;
#else
    #include <sys/socket.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
    #include <cerrno>
    using SocketHandle = int;
    constexpr SocketHandle INVALID_HANDLE = -1;
#endif

class SocketBaseError : public std::runtime_error {
public:
	explicit SocketBaseError(const std::string& msg) : std::runtime_error(msg) {}
};

class SocketDisconnectException : public SocketBaseError {
public:
	explicit SocketDisconnectException(const std::string& msg) : SocketBaseError(msg) {}
};

class SocketGuard {
public:
	SocketGuard(SocketHandle socket) : _socket(socket) {};
	SocketGuard() : _socket(INVALID_HANDLE) {};

	~SocketGuard() {
		if (_socket != INVALID_HANDLE) {
#ifdef _WIN32
			closesocket(_socket);
#else
			close(_socket);
#endif
		}
	}

	SocketGuard(SocketGuard&& other) noexcept {
		_socket = other._socket;
		other._socket = INVALID_HANDLE;
	}
	SocketGuard& operator=(SocketGuard&& other) noexcept {
		if (this != &other) {
			if (_socket != INVALID_HANDLE) {
#ifdef _WIN32
				closesocket(_socket);
#else
				close(_socket);
#endif
			}
			_socket = other._socket;
			other._socket = INVALID_HANDLE;
		}
		return *this;
	}

	SocketGuard(const SocketGuard&) = delete;
	SocketGuard& operator=(const SocketGuard&) = delete;

	void create(int addrFamily, int addrType, int addrProtocol);
	void bind(const sockaddr* addrName, int addrLength);
	void listen();
	void setTimeout(size_t seconds);
	bool isValid();
	SocketGuard accept();
	void send(const std::string& data);
	int recv(char* buffer, int size);

private:
	SocketHandle _socket;
};
