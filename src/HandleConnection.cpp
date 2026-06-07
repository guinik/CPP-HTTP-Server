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

void HandleConnection(SocketGuard socket, RadixTree& router) {

	std::string requestBytes = ReadRequest(socket);

	HTTPRequest request = parseRawBytesRequest(requestBytes);

	PrintRequest(request);

	std::optional<Handler> handler = router.match(request);
	HTTPResponse response;
	if(handler.has_value())
	{
		response = handler.value()(request);
	}
	else
	{
		response.code = "404";
		response.version = "HTTP/1.1";
		response.reason = "Path not found";
	}

	std::string rawResponse = HTTPResponseToRawString(response);
	socket.sendData(rawResponse);


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

std::string HTTPResponseToRawString(HTTPResponse& response) 
{
	std::string rawString = response.version + " " + response.code + " " + response.reason + "\r\n";
	for(auto& [key,val] : response.headers)
	{
		rawString += key + ": " + val + "\r\n";
	}
	rawString += "\r\n";
	rawString += response.body;

	return rawString;
};

