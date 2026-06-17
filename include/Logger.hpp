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

inline std::string timestamp() {
    auto now  = std::chrono::system_clock::now();
    auto tt   = std::chrono::system_clock::to_time_t(now);
    auto ms   = std::chrono::duration_cast<std::chrono::milliseconds>(
                    now.time_since_epoch()).count() % 1000;
    std::tm tm{};
#ifdef _WIN32
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    return std::format("{:04d}-{:02d}-{:02d} {:02d}:{:02d}:{:02d}.{:03d}",
        tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
        tm.tm_hour, tm.tm_min, tm.tm_sec, static_cast<long long>(ms));
}

inline void info(std::string_view msg) {
    std::lock_guard<std::mutex> lk(mutex());
    std::cout << '[' << timestamp() << "] INFO  " << msg << '\n';
}

inline void info(std::string_view requestId, std::string_view msg) {
    std::lock_guard<std::mutex> lk(mutex());
    std::cout << '[' << timestamp() << "] [" << requestId << "] INFO  " << msg << '\n';
}

inline void error(std::string_view msg) {
    std::lock_guard<std::mutex> lk(mutex());
    std::cerr << '[' << timestamp() << "] ERROR " << msg << '\n';
}

inline void error(std::string_view requestId, std::string_view msg) {
    std::lock_guard<std::mutex> lk(mutex());
    std::cerr << '[' << timestamp() << "] [" << requestId << "] ERROR " << msg << '\n';
}

} // namespace Log
