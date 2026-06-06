#include "HandleConnection.hpp"
#include <iostream>

void PrintRequest(const HTTPRequest& req) {
	std::cout << "=== HTTP REQUEST ===\n";
	std::cout << req.method << " " << req.path << " " << req.version << "\n\n";

	std::cout << "--- HEADERS ---\n";
	for (const auto& [key, value] : req.headers) {
		std::cout << key << ": " << value << "\n";
	}

	std::cout << "\n--- BODY ---\n";
	std::cout << req.body << "\n";

	std::cout << "====================\n";
}

void HandleConnection(SocketGuard socket) {

	std::string requestBytes = ReadRequest(socket);

	HTTPRequest request = parseRawBytesRequest(requestBytes);


	PrintRequest(request);
}

std::string ReadRequest(SocketGuard& socket) {

	std::string buffer;
	constexpr int bufferSize = 1024;
	char temp[bufferSize];


	while (true) {
		auto bytes = socket.recvData(temp, sizeof(temp));

		if (bytes <= 0) {
			break;
		}

		buffer.append(temp, bytes);

		if (buffer.find("\r\n\r\n") != std::string::npos) {
			//http request done
			break;
		}
	}
	return buffer;
}