#pragma once
#include <stdexcept>
#include <format>
#include <atomic>
#include "SocketGuard.hpp"
#include "AddrInfo.hpp"
#include "HandleConnection.hpp"
#include "Router.hpp"
#include "ThreadPool.hpp"

#ifdef _WIN32
class WinSocketGuard {
public:
	WinSocketGuard() {
		if (WSAStartup(MAKEWORD(2, 2), &_wsaData) != 0) {
			throw std::runtime_error(std::format("WSA Startup Failed"));
		}
	}
	~WinSocketGuard() {
		WSACleanup();
	}
	WinSocketGuard(const WinSocketGuard&) = delete;
	WinSocketGuard& operator=(const WinSocketGuard&) = delete;
private:
	WSADATA _wsaData;
};
#endif

class WSAHandler {
public:
	static constexpr size_t kMaxConnections = 1000;

	WSAHandler(const std::string& PORT, RadixTree& router, ThreadPool& threadPool, std::atomic_bool& running)
		: _PORT(PORT), _router(router), _threadPool(threadPool), _running(running) {}
	void run();

private:
#ifdef _WIN32
	WinSocketGuard _winSocketInitializer;
#endif
	std::string _PORT;
	RadixTree& _router;
	ThreadPool& _threadPool;
	std::atomic_bool& _running;
	std::atomic<size_t> _activeConnections{ 0 };
};