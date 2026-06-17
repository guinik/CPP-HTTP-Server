#pragma once
#include <iostream>
#include <mutex>
#include <string_view>
#include <chrono>
#include <ctime>
#include <format>

namespace Log {

inline std::mutex& mutex() {
    static std::mutex m;
    return m;
}

// ISO 8601 UTC timestamp with millisecond precision.
inline std::string timestamp() {
    auto now = std::chrono::system_clock::now();
    auto tt  = std::chrono::system_clock::to_time_t(now);
    auto ms  = std::chrono::duration_cast<std::chrono::milliseconds>(
                   now.time_since_epoch()).count() % 1000;
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    return std::format("{:04d}-{:02d}-{:02d}T{:02d}:{:02d}:{:02d}.{:03d}Z",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<long long>(ms));
}

// Escape a string for embedding inside a JSON double-quoted value.
inline std::string jsonStr(std::string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;      break;
        }
    }
    return out;
}

inline void info(std::string_view msg) {
    std::lock_guard<std::mutex> lk(mutex());
    std::cout << "{\"ts\":\"" << timestamp() << "\",\"level\":\"INFO\",\"msg\":\""
              << jsonStr(msg) << "\"}\n";
}

inline void info(std::string_view requestId, std::string_view msg) {
    std::lock_guard<std::mutex> lk(mutex());
    std::cout << "{\"ts\":\"" << timestamp() << "\",\"level\":\"INFO\",\"req\":\""
              << requestId << "\",\"msg\":\"" << jsonStr(msg) << "\"}\n";
}

inline void warn(std::string_view requestId, std::string_view msg) {
    std::lock_guard<std::mutex> lk(mutex());
    std::cerr << "{\"ts\":\"" << timestamp() << "\",\"level\":\"WARN\",\"req\":\""
              << requestId << "\",\"msg\":\"" << jsonStr(msg) << "\"}\n";
}

inline void error(std::string_view msg) {
    std::lock_guard<std::mutex> lk(mutex());
    std::cerr << "{\"ts\":\"" << timestamp() << "\",\"level\":\"ERROR\",\"msg\":\""
              << jsonStr(msg) << "\"}\n";
}

inline void error(std::string_view requestId, std::string_view msg) {
    std::lock_guard<std::mutex> lk(mutex());
    std::cerr << "{\"ts\":\"" << timestamp() << "\",\"level\":\"ERROR\",\"req\":\""
              << requestId << "\",\"msg\":\"" << jsonStr(msg) << "\"}\n";
}

} // namespace Log
