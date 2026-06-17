#include "HandleConnection.hpp"
#include "Logger.hpp"
#include "Metrics.hpp"
#include <algorithm>
#include <atomic>
#include <charconv>
#include <format>

static std::atomic<uint64_t> s_requestCounter{0};

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

static bool keepAliveMechanism(const HTTPRequest& request, HTTPResponse& response)
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

static std::string jsonError(const std::string& msg)
{
    return std::string(R"({"error":")") + Log::jsonStr(msg) + "\"}";
}

static bool lastEncodingIsChunked(const std::string& te)
{
    std::string lastTok;
    size_t pos = 0;
    while (true) {
        auto comma = te.find(',', pos);
        auto end   = (comma == std::string::npos) ? te.size() : comma;
        auto s     = te.find_first_not_of(" \t", pos);
        if (s != std::string::npos && s < end) {
            auto e = te.find_last_not_of(" \t", end - 1);
            if (e != std::string::npos)
                lastTok = te.substr(s, e - s + 1);
        }
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    std::transform(lastTok.begin(), lastTok.end(), lastTok.begin(),
        [](unsigned char c){ return static_cast<char>(std::tolower(c)); });
    return lastTok == "chunked";
}

static std::string ReadRequestBodyChunked(SocketGuard& socket, std::string& leftover,
                                           size_t maxBodyBytes)
{
    constexpr int kRecvSize = 1024;
    char temp[kRecvSize];
    std::string buf  = std::move(leftover);
    std::string body;

    auto fillUntilCRLF = [&]() {
        while (buf.find("\r\n") == std::string::npos) {
            auto n = socket.recv(temp, kRecvSize);
            buf.append(temp, static_cast<size_t>(n));
        }
    };

    auto fillUntil = [&](size_t needed) {
        while (buf.size() < needed) {
            auto n = socket.recv(temp, kRecvSize);
            buf.append(temp, static_cast<size_t>(n));
        }
    };

    constexpr size_t kMaxChunks = 65536;
    size_t chunkCount = 0;

    while (true) {
        fillUntilCRLF();
        auto crlfPos = buf.find("\r\n");

        std::string_view sizeField{buf.data(), crlfPos};
        auto extPos = sizeField.find(';');
        if (extPos != std::string_view::npos)
            sizeField = sizeField.substr(0, extPos);
        while (!sizeField.empty() && (sizeField.back() == ' ' || sizeField.back() == '\t'))
            sizeField.remove_suffix(1);

        size_t chunkSize = 0;
        auto [ptr, ec] = std::from_chars(sizeField.data(),
                                          sizeField.data() + sizeField.size(),
                                          chunkSize, 16);
        if (ec != std::errc{} || ptr != sizeField.data() + sizeField.size())
            throw BadRequestException("Malformed chunked encoding: invalid chunk size");

        buf.erase(0, crlfPos + 2);

        if (++chunkCount > kMaxChunks)
            throw BadRequestException(
                std::format("Too many chunks (limit {})", kMaxChunks));

        if (chunkSize == 0) {
            while (true) {
                fillUntilCRLF();
                auto pos        = buf.find("\r\n");
                bool isEmptyLine = (pos == 0);
                buf.erase(0, pos + 2);
                if (isEmptyLine) break;
            }
            break;
        }

        if (chunkSize > maxBodyBytes - body.size())
            throw PayloadTooLargeException(
                std::format("Chunked body exceeds {} byte limit", maxBodyBytes));

        fillUntil(chunkSize + 2);
        body.append(buf, 0, chunkSize);
        buf.erase(0, chunkSize + 2);
    }

    leftover = std::move(buf);
    return body;
}

void HandleConnection(SocketGuard socket, const IRouter& router, std::atomic_bool& running,
                      Metrics& metrics, const ServerConfig& config)
{
    socket.setTimeout(1);

    Log::info(std::format("Connection from {}", socket.peerAddress()));

    std::string leftover;
    size_t requestCount = 0;
    while (socket.isValid() && running)
    {
        HTTPResponse response;
        bool keepAlive = false;
        std::string requestId = std::format("{:08x}", ++s_requestCounter);
        ++metrics.requestsTotal;
        const auto reqStart = std::chrono::steady_clock::now();

        try {
            std::string requestBytes = ReadRequestHead(socket, leftover,
                                                        config.maxHeaderBytes,
                                                        config.headerTimeout);

            HTTPHead head = parseRawBytesHeadRequest(requestBytes, config.maxUriBytes);

            if (head.version == "HTTP/1.1") {
                auto hostIt = head.headers.find("Host");
                if (hostIt == head.headers.end() || hostIt->second.empty())
                    throw BadRequestException("HTTP/1.1 request must include a Host header");
            }

            std::string requestBodyBytes;
            auto teIt = head.headers.find("Transfer-Encoding");
            if (teIt != head.headers.end()) {
                if (!lastEncodingIsChunked(teIt->second))
                    throw BadRequestException(
                        std::format("Unsupported Transfer-Encoding: {}", teIt->second));
                requestBodyBytes = ReadRequestBodyChunked(socket, leftover, config.maxBodyBytes);
            } else {
                size_t bodyBytes = 0;
                if (head.headers.count("Content-Length") != 0) {
                    const std::string& clStr = head.headers["Content-Length"];
                    if (!clStr.empty() && clStr[0] == '-')
                        throw BadRequestException("Negative Content-Length");
                    try {
                        unsigned long long parsed = std::stoull(clStr);
                        if (parsed > config.maxBodyBytes)
                            throw PayloadTooLargeException(
                                std::format("Body exceeds {} byte limit", config.maxBodyBytes));
                        bodyBytes = static_cast<size_t>(parsed);
                    }
                    catch (const BadRequestException&)      { throw; }
                    catch (const PayloadTooLargeException&) { throw; }
                    catch (std::exception& e) {
                        throw BadRequestException(
                            std::format("Invalid Content-Length: {}", e.what()));
                    }
                }
                requestBodyBytes = ReadRequestBody(socket, bodyBytes, leftover);
            }

            auto it = head.headers.find("Content-Type");
            std::string contentType = it != head.headers.end() ? it->second : "";

            HTTPBody body = parseRawBytesBodyRequest(requestBodyBytes, contentType);
            HTTPRequest request = constructRequest(head, body);
            request.head.requestId = requestId;

            RouteMatch matchResult = router.match(request);
            request.head.params      = std::move(matchResult.params);
            request.head.queryParams = std::move(matchResult.queryParams);

            if (matchResult.route)
            {
                const auto handlerStart = std::chrono::steady_clock::now();
                matchResult.route->composedChain(request, response);
                const auto elapsed = std::chrono::steady_clock::now() - handlerStart;
                if (elapsed > config.handlerTimeout) {
                    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count();
                    Log::warn(requestId,
                        std::format("Handler took {}ms (limit {}s); closing connection",
                                    ms, config.handlerTimeout.count()));
                    keepAlive = false;
                }
            }
            else if (matchResult.pathFound)
            {
                response.code   = "405";
                response.reason = "Method Not Allowed";
                response.body   = jsonError("Method Not Allowed");
                response.headers["Content-Type"] = "application/json";
                std::string allow;
                for (size_t i = 0; i < matchResult.allowedMethods.size(); ++i) {
                    if (i > 0) allow += ", ";
                    allow += matchResult.allowedMethods[i];
                }
                response.headers["Allow"] = allow;
            }
            else
            {
                response.code   = "404";
                response.reason = "Not Found";
                response.body   = jsonError("Not Found");
                response.headers["Content-Type"] = "application/json";
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
            response.code   = "400";
            response.reason = "Bad Request";
            response.body   = jsonError(e.what());
            response.headers["Content-Type"]  = "application/json";
            response.headers["Connection"]    = "close";
        }
        catch (const RequestUriTooLongException& e) {
            Log::error(requestId, std::format("URI too long: {}", e.what()));
            response.code   = "414";
            response.reason = "URI Too Long";
            response.body   = jsonError(e.what());
            response.headers["Content-Type"]  = "application/json";
            response.headers["Connection"]    = "close";
        }
        catch (const PayloadTooLargeException& e) {
            Log::error(requestId, std::format("Payload too large: {}", e.what()));
            response.code   = "413";
            response.reason = "Payload Too Large";
            response.body   = jsonError(e.what());
            response.headers["Content-Type"]  = "application/json";
            response.headers["Connection"]    = "close";
        }
        catch (const RequestHeaderFieldsTooLargeException& e) {
            Log::error(requestId, std::format("Headers too large: {}", e.what()));
            response.code   = "431";
            response.reason = "Request Header Fields Too Large";
            response.body   = jsonError(e.what());
            response.headers["Content-Type"]  = "application/json";
            response.headers["Connection"]    = "close";
        }
        catch (const HttpVersionNotSupportedException& e) {
            Log::error(requestId, std::format("HTTP version not supported: {}", e.what()));
            response.code   = "505";
            response.reason = "HTTP Version Not Supported";
            response.body   = jsonError(e.what());
            response.headers["Content-Type"]  = "application/json";
            response.headers["Connection"]    = "close";
        }
        catch (const RequestTimeoutException& e) {
            Log::error(requestId, std::format("Request timeout: {}", e.what()));
            response.code   = "408";
            response.reason = "Request Timeout";
            response.body   = jsonError(e.what());
            response.headers["Content-Type"]  = "application/json";
            response.headers["Connection"]    = "close";
        }
        catch (const std::exception& e) {
            Log::error(requestId, std::format("Unhandled exception: {}", e.what()));
            response.code   = "500";
            response.reason = "Internal Server Error";
            response.body   = jsonError("Internal Server Error");
            response.headers["Content-Type"]  = "application/json";
            response.headers["Connection"]    = "close";
        }

        if (!response.code.empty()) {
            char cls = response.code[0];
            if      (cls == '2') ++metrics.responses2xx;
            else if (cls == '4') ++metrics.responses4xx;
            else if (cls == '5') ++metrics.responses5xx;
        }

        socket.send(HTTPResponseToRawString(response));

        metrics.recordLatency(static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - reqStart).count()));

        if (!keepAlive || ++requestCount >= config.maxRequestsPerConn) break;
    }
}

std::string ReadRequestHead(SocketGuard& socket, std::string& leftover,
                             size_t maxHeaderBytes,
                             std::chrono::seconds headerTimeout)
{
    const auto deadline = std::chrono::steady_clock::now() + headerTimeout;

    std::string buffer = std::move(leftover);
    leftover.clear();

    constexpr int bufferSize = 1024;
    char temp[bufferSize];

    while (true) {
        auto pos = buffer.find("\r\n\r\n");
        if (pos != std::string::npos) {
            leftover = buffer.substr(pos + 4);
            buffer.resize(pos + 4);
            return buffer;
        }

        if (buffer.size() > maxHeaderBytes)
            throw RequestHeaderFieldsTooLargeException("Request headers too large");

        if (std::chrono::steady_clock::now() > deadline)
            throw RequestTimeoutException("Header read timed out");

        auto bytes = socket.recv(temp, sizeof(temp));
        buffer.append(temp, static_cast<size_t>(bytes));
    }
}

std::string ReadRequestBody(SocketGuard& socket, size_t bodySize, std::string& leftover)
{
    if (bodySize == 0) return "";

    constexpr int bufferSize = 1024;
    char temp[bufferSize];
    std::string buffer = std::move(leftover);

    while (buffer.size() < bodySize) {
        auto bytes = socket.recv(temp, sizeof(temp));
        buffer.append(temp, static_cast<size_t>(bytes));
    }

    leftover = buffer.substr(bodySize);
    buffer.resize(bodySize);
    return buffer;
}
