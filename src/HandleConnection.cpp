#include "HandleConnection.hpp"
#include "Logger.hpp"
#include <format>

bool keepAliveMechanism(const HTTPRequest& request, HTTPResponse& response)
{
	auto keepAlive = (request.head.version == "HTTP/1.1");
	auto it = request.head.headers.find("Connection");
	if (it != request.head.headers.end())
	{
		std::string connVal = it->second;
		std::transform(connVal.begin(), connVal.end(), connVal.begin(),
			[](unsigned char c) { return std::tolower(c); });
		keepAlive = (connVal == "keep-alive");
	}

	response.headers["Connection"] = keepAlive ? "keep-alive" : "close";

	return keepAlive;

};

void HandleConnection(SocketGuard socket, RouteTrie& router, std::atomic_bool& running) {
	// 1 s recv timeout: a blocked recv wakes within 1 s so the loop condition
	// can re-evaluate _running after a shutdown signal.
	socket.setTimeout(1);

	while (socket.isValid() && running)
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
					int parsed = std::stoi(head.headers["Content-Length"].c_str());
					if (parsed < 0)
						throw BadRequestException("Negative Content-Length");
					bodyBytes = static_cast<size_t>(parsed);
				}
				catch (const BadRequestException&) {
					throw;
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

			auto [route, pathFound] = router.match(request);

			if (route)
			{
				applyRoute(route->middleware, request, response, route->handler);
			}
			else if (pathFound)
			{
				response.code = "405";
				response.reason = "Method Not Allowed";
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
			Log::info(std::format("Connection finished: {}", e.what()));
			return;
		}
		catch (const BadRequestException& e) {
			Log::error(std::format("Bad request: {}", e.what()));
			response.code = "400";
			response.reason = "Bad Request";
		}
		catch (const std::exception& e) {
			Log::error(std::format("Connection error: {}", e.what()));
			response.code = "500";
			response.reason = "Server Error";
		}

		std::string rawResponse = HTTPResponseToRawString(response);
		socket.send(rawResponse);

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
		auto bytes = socket.recv(temp, sizeof(temp));

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
		auto bytes = socket.recv(temp, sizeof(temp));

		if (bytes <= 0) {
			break;
		}

		buffer.append(temp, static_cast<size_t>(bytes));

	}
	buffer.resize(bodySize);
	return buffer;
}

