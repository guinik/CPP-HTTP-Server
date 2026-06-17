#include "HandleConnection.hpp"
#include "Logger.hpp"
#include <algorithm>
#include <atomic>
#include <format>

static std::atomic<uint64_t> s_requestCounter{0};

// Returns true if a comma-separated token list contains the given token (case-folded).
static bool hasConnectionToken(const std::string& fieldValue, const std::string& token)
{
	size_t pos = 0;
	while (pos <= fieldValue.size()) {
		auto end = fieldValue.find(',', pos);
		if (end == std::string::npos) end = fieldValue.size();
		auto s = fieldValue.find_first_not_of(" \t", pos);
		if (s != std::string::npos && s < end) {
			auto e = fieldValue.find_last_not_of(" \t", end - 1);
			std::string tok = fieldValue.substr(s, e - s + 1);
			std::transform(tok.begin(), tok.end(), tok.begin(),
				[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (tok == token) return true;
		}
		pos = end + 1;
	}
	return false;
}

bool keepAliveMechanism(const HTTPRequest& request, HTTPResponse& response)
{
	auto keepAlive = (request.head.version == "HTTP/1.1");
	auto it = request.head.headers.find("Connection");
	if (it != request.head.headers.end()) {
		if (hasConnectionToken(it->second, "close"))
			keepAlive = false;
		else if (hasConnectionToken(it->second, "keep-alive"))
			keepAlive = true;
	}

	response.headers["Connection"] = keepAlive ? "keep-alive" : "close";

	return keepAlive;
}

void HandleConnection(SocketGuard socket, RouteTrie& router, std::atomic_bool& running) {
	// 1 s recv timeout: a blocked recv wakes within 1 s so the loop condition
	// can re-evaluate _running after a shutdown signal.
	socket.setTimeout(1);

	while (socket.isValid() && running)
	{
		HTTPResponse response;
		bool keepAlive = false;
		std::string requestId = std::format("{:08x}", ++s_requestCounter);

		try {
			auto [requestBytes, leftover] = ReadRequestHead(socket);

			HTTPHead head = parseRawBytesHeadRequest(requestBytes);
			size_t bodyBytes{ 0 };
			if (head.headers.count("Content-Length") != 0)
			{
				const std::string& clStr = head.headers["Content-Length"];
				if (!clStr.empty() && clStr[0] == '-')
					throw BadRequestException("Negative Content-Length");
				try
				{
					unsigned long long parsed = std::stoull(clStr);
					if (parsed > 10ULL * 1024 * 1024)
						throw PayloadTooLargeException("Body too large");
					bodyBytes = static_cast<size_t>(parsed);
				}
				catch (const BadRequestException&) {
					throw;
				}
				catch (const PayloadTooLargeException&) {
					throw;
				}
				catch (std::exception& e) {
					throw BadRequestException(std::format("Invalid Content-Length: {}", e.what()));
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
			request.head.requestId = requestId;

			RouteMatch match = router.match(request);

			if (match.route)
			{
				applyRoute(match.route->middleware, request, response, match.route->handler);
			}
			else if (match.pathFound)
			{
				response.code = "405";
				response.reason = "Method Not Allowed";
				std::string allow;
				for (size_t i = 0; i < match.allowedMethods.size(); ++i) {
					if (i > 0) allow += ", ";
					allow += match.allowedMethods[i];
				}
				response.headers["Allow"] = allow;
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
			Log::info(requestId, std::format("Connection finished: {}", e.what()));
			return;
		}
		catch (const BadRequestException& e) {
			Log::error(requestId, std::format("Bad request: {}", e.what()));
			response.code = "400";
			response.reason = "Bad Request";
			response.headers["Connection"] = "close";
		}
		catch (const RequestUriTooLongException& e) {
			Log::error(requestId, std::format("URI too long: {}", e.what()));
			response.code = "414";
			response.reason = "URI Too Long";
			response.headers["Connection"] = "close";
		}
		catch (const PayloadTooLargeException& e) {
			Log::error(requestId, std::format("Payload too large: {}", e.what()));
			response.code = "413";
			response.reason = "Payload Too Large";
			response.headers["Connection"] = "close";
		}
		catch (const RequestHeaderFieldsTooLargeException& e) {
			Log::error(requestId, std::format("Headers too large: {}", e.what()));
			response.code = "431";
			response.reason = "Request Header Fields Too Large";
			response.headers["Connection"] = "close";
		}
		catch (const HttpVersionNotSupportedException& e) {
			Log::error(requestId, std::format("HTTP version not supported: {}", e.what()));
			response.code = "505";
			response.reason = "HTTP Version Not Supported";
			response.headers["Connection"] = "close";
		}
		catch (const std::exception& e) {
			Log::error(requestId, std::format("Connection error: {}", e.what()));
			response.code = "500";
			response.reason = "Internal Server Error";
			response.headers["Connection"] = "close";
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

		buffer.append(temp, static_cast<size_t>(bytes));

		if (buffer.size() > maxHeaderSize) {
			throw RequestHeaderFieldsTooLargeException("Request headers too large");
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

	constexpr int bufferSize = 1024;
	char temp[bufferSize];
	std::string buffer = std::move(leftover);

	while (true) {
		if (buffer.size() >= bodySize) {
			break;
		}
		auto bytes = socket.recv(temp, sizeof(temp));
		buffer.append(temp, static_cast<size_t>(bytes));
	}
	buffer.resize(bodySize);
	return buffer;
}
