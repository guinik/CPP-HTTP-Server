cmake_minimum_required(VERSION 3.20)
project(HTTPSERVER)



set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SERVER_SOURCES
    src/HttpServer.cpp
    src/SocketGuard.cpp
    src/HandleConnection.cpp
    src/HTTPRequest.cpp
    src/HTTPSerializer.cpp
    src/Router.cpp
    src/UserRoutes.cpp
    src/ThreadPool.cpp
)

set(SERVER_HEADERS
    include/HttpServer.hpp
    include/SocketGuard.hpp
    include/AddrInfo.hpp
    include/HandleConnection.hpp
    include/HTTPRequest.hpp
    include/HTTPResponse.hpp
    include/HTTPErrors.hpp
    include/IRouter.hpp
    include/RouteTypes.hpp
    include/Router.hpp
    include/UserRoutes.hpp
    include/Utils.hpp
    include/Cors.hpp
    include/ThreadPool.hpp
    include/StringUtils.hpp
    include/Logger.hpp
    include/ServerConfig.hpp
    include/Metrics.hpp
)

source_group("Sources" FILES ${SERVER_SOURCES})
source_group("Headers" FILES ${SERVER_HEADERS})

add_library(http_server_lib STATIC ${SERVER_SOURCES} ${SERVER_HEADERS})

target_include_directories(http_server_lib PUBLIC include)

if(MSVC)
    target_compile_options(http_server_lib PRIVATE /W4 /WX)
else()
    target_compile_options(http_server_lib PRIVATE -Wall -Wextra -Werror)
endif()

if(WIN32)
    target_link_libraries(http_server_lib ws2_32)
endif()

add_executable(http_server src/main.cpp)
target_link_libraries(http_server PRIVATE http_server_lib)

if(MSVC)
    target_compile_options(http_server PRIVATE /W4 /WX)
else()
    target_compile_options(http_server PRIVATE -Wall -Wextra -Werror)
endif()






# ── Optional: ASAN + UBSAN (GCC / Clang only) ────────────────────────────────
option(ENABLE_SANITIZERS "Compile with AddressSanitizer + UBSan (GCC/Clang only)" OFF)
if(ENABLE_SANITIZERS AND NOT MSVC)
    set(SAN_FLAGS -fsanitize=address,undefined -fno-omit-frame-pointer)
    foreach(tgt IN ITEMS http_server_lib http_server)
        target_compile_options(${tgt} PRIVATE ${SAN_FLAGS})
        target_link_options(${tgt}    PRIVATE ${SAN_FLAGS})
    endforeach()
endif()

# ── Optional: libFuzzer harness (Clang only) ─────────────────────────────────
option(ENABLE_FUZZER "Build libFuzzer harness — requires Clang" OFF)
if(ENABLE_FUZZER AND NOT MSVC)
    add_executable(fuzz_http_parser tests/fuzz_http_parser.cpp)
    target_include_directories(fuzz_http_parser PRIVATE include)
    target_link_libraries(fuzz_http_parser PRIVATE http_server_lib)
    set(FUZZ_FLAGS -fsanitize=fuzzer,address,undefined -fno-omit-frame-pointer)
    target_compile_options(fuzz_http_parser PRIVATE ${FUZZ_FLAGS})
    target_link_options(fuzz_http_parser    PRIVATE ${FUZZ_FLAGS})
endif()

# --- Tests ---
enable_testing()
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG        v1.15.2
)
FetchContent_MakeAvailable(googletest)


add_executable(unit_tests
    tests/test_parser.cpp
    tests/test_router.cpp
    tests/test_middleware.cpp
    tests/test_threadpool.cpp
    tests/test_serializer.cpp
    tests/test_connection.cpp
)

target_link_libraries(unit_tests PRIVATE http_server_lib GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(unit_tests)

