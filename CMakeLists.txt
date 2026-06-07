cmake_minimum_required(VERSION 3.20)
project(HTTPSERVER)



set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(HTTPSERVER
    src/main.cpp
    src/WSA.cpp
    src/SocketGuard.cpp
	src/HandleConnection.cpp
	src/HTTPRequest.cpp
	src/Router.cpp
	src/UserRoutes.cpp
	include/WSA.hpp
    include/SocketGuard.hpp
	include/AddrInfo.hpp
	include/HandleConnection.hpp
	include/HTTPRequest.hpp
	include/Router.hpp
	include/HTTPResponse.hpp
	include/UserRoutes.hpp
)
target_include_directories(HTTPSERVER PRIVATE include)

target_link_libraries(HTTPSERVER ws2_32)
