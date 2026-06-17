#pragma once
#include <iostream>
#include <mutex>
#include <string_view>

namespace Log {

inline std::mutex& mutex() {
    static std::mutex m;
    return m;
}

inline void info(std::string_view msg) {
    std::lock_guard<std::mutex> lk(mutex());
    std::cout << msg << '\n';
}

inline void error(std::string_view msg) {
    std::lock_guard<std::mutex> lk(mutex());
    std::cerr << msg << '\n';
}

} // namespace Log
