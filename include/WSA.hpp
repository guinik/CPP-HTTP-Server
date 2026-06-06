
#include<winsock2.h>
#include <ws2tcpip.h>
#include <stdexcept>
#include <format>
#include "SocketGuard.hpp"
#include "AddrInfo.hpp"

class WSAHandlerBaseError : public std::runtime_error {
public:
	explicit WSAHandlerBaseError(const std::string& msg) : std::runtime_error(msg) {
	}
};

class WSAStartUpError : public WSAHandlerBaseError {
public:
	explicit WSAStartUpError(const std::string& msg) : WSAHandlerBaseError(msg) {
	}
};

class WSACantGetSocket : public WSAHandlerBaseError {
public:
	explicit WSACantGetSocket(const std::string& msg) : WSAHandlerBaseError(msg) {
	}
};

class WSAInvalidListenSocket : public WSAHandlerBaseError {
public:
	explicit WSAInvalidListenSocket(const std::string& msg) : WSAHandlerBaseError(msg) {
	}
};
class WSABindSocketError : public WSAHandlerBaseError {
public:
	explicit WSABindSocketError(const std::string& msg) : WSAHandlerBaseError(msg) {
	}
};

class WSAListenError : public WSAHandlerBaseError {
public:
	explicit WSAListenError(const std::string& msg) : WSAHandlerBaseError(msg) {
	}
};

class WSAAcceptFailed : public WSAHandlerBaseError {
public:
	explicit WSAAcceptFailed(const std::string& msg) : WSAHandlerBaseError(msg) {
	}
};

class WinSocketGuard {
public:
	WinSocketGuard() {

		if (WSAStartup(MAKEWORD(2, 2), &_wsaData) != 0) {
			throw WSAStartUpError(std::format("WSA Startup Failed"));
		}

	};
	~WinSocketGuard() {
		WSACleanup();
	};


	WinSocketGuard(const WinSocketGuard&) = delete;
	WinSocketGuard& operator=(const WinSocketGuard&) = delete;

private:
	WSADATA _wsaData;
};

class WSAHandler {
public:
	WSAHandler(const std::string& PORT) : _PORT(PORT) {};
	void run();
private:
	WinSocketGuard _winSocketInitializer;
	std::string _PORT;
};