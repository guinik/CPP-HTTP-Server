#pragma once

#ifdef _WIN32
    #include <winsock2.h>
    #include <ws2tcpip.h>
#else
    #include <sys/socket.h>
    #include <netdb.h>
#endif
#include <stdexcept>
#include <format>
class AddrInfoGuard {
public:
    AddrInfoGuard(const char* node, const char* service, addrinfo* hints) {
        if (getaddrinfo(node, service, hints, &_ptr) != 0) {
            throw std::runtime_error("getaddrinfo failed");
        }
    }

    ~AddrInfoGuard() {
        if (_ptr) freeaddrinfo(_ptr);
    }

    addrinfo* get() const { return _ptr; }

private:
    addrinfo* _ptr = nullptr;
};