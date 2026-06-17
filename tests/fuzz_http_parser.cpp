// libFuzzer entry point for the HTTP head parser.
//
// Build (Clang only):
//   cmake -DENABLE_FUZZER=ON -DCMAKE_CXX_COMPILER=clang++ ..
//   cmake --build . --target fuzz_http_parser
//   ./fuzz_http_parser corpus/ -max_total_time=60
//
// ASAN + UBSAN are injected automatically by the ENABLE_FUZZER CMake option.
#include <cstdint>
#include <cstddef>
#include "HTTPRequest.hpp"

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    std::string_view raw{reinterpret_cast<const char*>(data), size};
    try {
        parseRawBytesHeadRequest(raw);
    } catch (...) {
        // All exceptions are expected protocol errors.
        // A crash (detected by ASAN/UBSAN) is a real bug.
    }
    return 0;
}
