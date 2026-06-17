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
    include/Router.hpp
    include/UserRoutes.hpp
    include/Utils.hpp
    include/Cors.hpp
    include/ThreadPool.hpp
    include/StringUtils.hpp
    include/Logger.hpp
)

source_group("Sources" FILES ${SERVER_SOURCES})
source_group("Headers" FILES ${SERVER_HEADERS})

add_library(http_server_lib STATIC ${SERVER_SOURCES} ${SERVER_HEADERS})

target_include_directories(http_server_lib PUBLIC include)

if(WIN32)
    target_link_libraries(http_server_lib ws2_32)
endif()



add_executable(http_server src/main.cpp)
target_link_libraries(http_server PRIVATE http_server_lib)






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

