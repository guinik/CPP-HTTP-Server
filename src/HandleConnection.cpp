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

	std::cout << "--- PARAM HEADERS ---\n";
	for (const auto& [key, value] : req.params) {
		std::cout << key << ": " << value << "\n";
	}

	std::cout << "--- QUERY PARAMS ---\n";
	for (const auto& [key, value] : req.queryParams) {
		std::cout << key << ": " << value << "\n";
	}

	std::cout << "\n--- BODY ---\n";
	std::cout << requestWBody.body.raw << "\n";

	std::cout << "====================\n";
}

bool keepAliveMechanism(const HTTPRequest& request, HTTPResponse& response)
{
	auto keepAlive = (request.head.version == "HTTP/1.1");
	auto it = request.head.headers.find("Connection");
	if (it != request.head.headers.end())
	{
		keepAlive = (it->second == "keep-alive");
	}

	response.headers["Connection"] = keepAlive ? "keep-alive" : "close";

	return keepAlive;

};

void HandleConnection(SocketGuard socket, RadixTree& router) {
	size_t timeoutSeconds{ 5 };
	socket.setTimeout(timeoutSeconds);

	while (socket.isValid())
	{
		HTTPResponse response;
		bool keepAlive = false;

		try {
			auto [requestBytes, leftover] = ReadRequestHead(socket);

			HTTPHead head = parseRawBytesHeadRequest(requestBytes);
			size_t bodyBytes{ 0 };
			if (head.headers.count("Content-Length") != 0)
			{	
				try
				{
					bodyBytes = std::stoi(head.headers["Content-Length"].c_str());
				}
				catch (std::exception& e) {
					throw std::runtime_error(std::format("Parsing content-length failed: {}", e.what()));
				}
			}

			std::string requestBodyBytes = ReadRequestBody(socket, bodyBytes, leftover);

			auto it = head.headers.find("Content-Type");
			std::string contentType = "";
			if (it != head.headers.end())
			{
				contentType = it->second;
			}



			HTTPBody body = parseRawBytesBodyRequest(requestBodyBytes, contentType);

			HTTPRequest request = constructRequest(head, body);

			RadixTreeNode* routePointer = router.match(request);
			//PrintRequest(request);


			if(routePointer)
			{
				auto it = routePointer->routeMap.find(request.head.method);

				if (it != routePointer->routeMap.end())
				{
					applyRoute(it->second.middleware, request, response, it->second.handler);
				}
				else
				{
					response.code = "405";
					response.reason = "Method Not Allowed";
				}

			
			}
			else
			{
				response.code = "404";
				response.reason = "Path not found";
			
			}
			keepAlive = keepAliveMechanism(request, response);

		}
		catch (const SocketDisconnectException& e)
		{
			std::cerr << "Connection finished: " << e.what() << "\n";
			return;
		}
		catch (const std::exception& e) {
			std::cerr << "Connection error: " << e.what() << "\n";
			response.code = "500";
			response.reason = "Server Error";

		}

		std::string rawResponse = HTTPResponseToRawString(response);
		socket.sendData(rawResponse);

		if (!keepAlive)
		{
			break;
		};

	}
}

std::pair<std::string, std::string> ReadRequestHead(SocketGuard& socket) {

	std::string buffer;
	std::string leftover;
	constexpr int bufferSize = 1024;
	char temp[bufferSize];


	constexpr size_t maxHeaderSize = 8 * 1024;
	while (true) {
		auto bytes = socket.recvData(temp, sizeof(temp));

		if (bytes <= 0) {
			break;
		}

		buffer.append(temp, static_cast<size_t>(bytes));

		if (buffer.size() > maxHeaderSize) {
			throw std::runtime_error("Request headers too large");
		}

		auto pos = buffer.find("\r\n\r\n");
		if (pos != std::string::npos) {
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

	if (bodySize > 1024 * 1024 * 10) { // 10MB max
		throw std::runtime_error("Body too large");
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

		buffer.append(temp, static_cast<size_t>(bytes));

	}
	buffer.resize(bodySize);
	return buffer;
}

void addTimeHeader(HTTPResponse& response)
{
	auto now = std::chrono::system_clock::now();
	auto time = std::chrono::system_clock::to_time_t(now);
	std::tm tm{};
#ifdef _WIN32
	gmtime_s(&tm, &time);
#else
	gmtime_r(&time, &tm);
#endif
	char buf[64];
	std::strftime(buf, sizeof(buf), "%a, %d %b %Y %H:%M:%S GMT", &tm);
	response.headers["Date"] = buf;
}

std::string HTTPResponseToRawString(HTTPResponse& response) 
{
	addTimeHeader(response);
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