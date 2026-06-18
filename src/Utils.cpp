#include "Utils.hpp"
#include <chrono>
#include <format>
#include "Logger.hpp"

MiddleWare parseJson = [](HTTPRequest& req, HTTPResponse& res, Next next) {

	if (req.body.raw.empty())
	{
		next();
		return;
	}
	auto ctIt = req.head.headers.find("Content-Type");
	if (ctIt != req.head.headers.end())
	{
		// Strip parameters (e.g. "; charset=utf-8") before comparing the media type.
		const std::string& ctVal = ctIt->second;
		std::string mediaType = ctVal.substr(0, ctVal.find(';'));
		auto end = mediaType.find_last_not_of(' ');
		if (end != std::string::npos) mediaType = mediaType.substr(0, end + 1);
		if (mediaType != "application/json")
		{
			res.code = "415";
			res.reason = "Unsupported Media Type";
			return;
		}
	}
	try {
		req.body.json = nlohmann::json::parse(req.body.raw);
		next();
		return;
	}
	catch (const nlohmann::json::parse_error&) {
		res.code = "400";
		res.reason = "Invalid Json";
	}
	};



MiddleWare requestLogger = [](HTTPRequest& req, HTTPResponse& res, Next next) {
	auto start = std::chrono::steady_clock::now();
	try{
		next();
	}
	catch(const std::exception&)
	{
		throw;
	}


	auto end = std::chrono::steady_clock::now();
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	Log::info(req.head.requestId,
		std::format("{} {} {} {}ms", req.head.method, req.head.path, res.code, ms));
	};
