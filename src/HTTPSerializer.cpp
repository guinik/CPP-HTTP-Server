#include "HTTPSerializer.hpp"
#include <chrono>
#include <ctime>
#include <algorithm>

static std::string makeTimeHeader()
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
	return buf;
}

static std::string stripCRLF(std::string s)
{
	s.erase(std::remove_if(s.begin(), s.end(), [](char c){ return c == '\r' || c == '\n'; }), s.end());
	return s;
}

std::string HTTPResponseToRawString(const HTTPResponse& response)
{
	std::string rawString = stripCRLF(response.version) + " "
		+ stripCRLF(response.code) + " "
		+ stripCRLF(response.reason) + "\r\n";
	rawString += std::string("Date: ") + makeTimeHeader() + "\r\n";
	if (!response.body.empty())
		rawString += "Content-Length: " + std::to_string(response.body.size()) + "\r\n";
	for (const auto& [key, val] : response.headers)
		rawString += stripCRLF(key) + ": " + stripCRLF(val) + "\r\n";
	rawString += "\r\n";
	rawString += response.body;
	return rawString;
}
