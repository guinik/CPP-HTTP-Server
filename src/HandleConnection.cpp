#include "HandleConnection.hpp"
#include <iostream>

void PrintRequest(const HTTPRequest& requestWBody) {
	auto req = requestWBody.head;
	std::cout << "=== HTTP REQUEST ===\n";
	std::cout << req.method << " " << req.path << " " << req.version << "\n\n";

	std::cout << "--- HEADERS ---\n";
	for (const auto& [key, value] : req.headers) {
		std::cout << key << ": " << value << "\n";
	}

	std::cout << "\n--- BODY ---\n";
	std::cout << requestWBody.body.raw << "\n";

	std::cout << "====================\n";
}

void HandleConnection(SocketGuard socket, RadixTree& router) {

	auto [requestBytes, leftover] = ReadRequestHead(socket);

	HTTPHead head = parseRawBytesHeadRequest(requestBytes);
	size_t bodyBytes{ 0 };
	if (head.headers.count("Content-Length") != 0)
	{
		bodyBytes = std::stoi(head.headers["Content-Length"].c_str());
	}

	std::string requestBodyBytes = ReadRequestBody(socket, bodyBytes, leftover);
	
	auto it = head.headers.find("Content-Type");
	std::string contentType = "";
	if(it != head.headers.end())
	{
		contentType = it->second;
	}



	HTTPBody body = parseRawBytesBodyRequest(requestBodyBytes, contentType);

	HTTPRequest request = constructRequest(head, body);

	std::optional<Route> route = router.match(request);
	
	
	
	//PrintRequest(request);
	HTTPResponse response;
	if(route.has_value())
	{
		applyRoute(route.value().middleware, request, response, route.value().handler);
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

std::pair<std::string, std::string> ReadRequestHead(SocketGuard& socket) {

	std::string buffer;
	std::string leftover;
	constexpr int bufferSize = 1024;
	char temp[bufferSize];


	while (true) {
		auto bytes = socket.recvData(temp, sizeof(temp));

		if (bytes <= 0) {
			break;
		}

		buffer.append(temp, bytes);
		auto pos = buffer.find("\r\n\r\n");
		if (pos != std::string::npos) {
			//http request header done
			leftover = buffer.substr(pos + 4);
			buffer = buffer.substr(0, pos + 4);

			break;
		}
	}

	return { buffer , leftover };
}

std::string ReadRequestBody(SocketGuard& socket, size_t bodySize, std::string& leftover) {
	if (bodySize == 0)
	{
		return "";
	}
	constexpr int bufferSize = 1024;
	char temp[bufferSize];
	std::string buffer = std::move(leftover);


	while (true) {
		if (buffer.size() >= bodySize) {
			break;
		}
		auto bytes = socket.recvData(temp, sizeof(temp));

		if (bytes <= 0) {
			break;
		}

		buffer.append(temp, bytes);

	}
	buffer.resize(bodySize);
	return buffer;
}


std::string HTTPResponseToRawString(HTTPResponse& response) 
{
	if (!response.body.empty()) {
		response.headers["Content-Length"] = std::to_string(response.body.size());
	}
	std::string rawString = response.version + " " + response.code + " " + response.reason + "\r\n";
	for(auto& [key,val] : response.headers)
	{
		rawString += key + ": " + val + "\r\n";
	}
	rawString += "\r\n";
	rawString += response.body;

	return rawString;
};

