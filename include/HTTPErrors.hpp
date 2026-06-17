#pragma once
#include <stdexcept>
#include <string>

class BadRequestException : public std::runtime_error {
public:
    explicit BadRequestException(const std::string& msg) : std::runtime_error(msg) {}
};

class PayloadTooLargeException : public std::runtime_error {
public:
    explicit PayloadTooLargeException(const std::string& msg) : std::runtime_error(msg) {}
};

class RequestHeaderFieldsTooLargeException : public std::runtime_error {
public:
    explicit RequestHeaderFieldsTooLargeException(const std::string& msg) : std::runtime_error(msg) {}
};

class RequestUriTooLongException : public std::runtime_error {
public:
    explicit RequestUriTooLongException(const std::string& msg) : std::runtime_error(msg) {}
};

class HttpVersionNotSupportedException : public std::runtime_error {
public:
    explicit HttpVersionNotSupportedException(const std::string& msg) : std::runtime_error(msg) {}
};

class RequestTimeoutException : public std::runtime_error {
public:
    explicit RequestTimeoutException(const std::string& msg) : std::runtime_error(msg) {}
};
