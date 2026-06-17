#pragma once
#include <chrono>
#include <format>
#include "Router.hpp"
#include "Logger.hpp"

inline MiddleWare parseJson = [](HTTPRequest& req, HTTPResponse& res, Next next) {

	if (req.body.raw.empty())
	{
		next();
		return;
	}
	if (req.head.headers.count("Content-Type") && req.head.headers["Content-Type"] != "application/json")
	{
		res.code = "415";
		res.reason = "Unsupported Media Type";
		return;
	};
	try {
		req.body.json = nlohmann::json::parse(req.body.raw);
		next();
		return;
	}
	catch (nlohmann::json::parse_error& e ){
		res.code = "400";
		res.reason = "Invalid Json";
	}
};



inline MiddleWare requestLogger = [](HTTPRequest& req, HTTPResponse& res, Next next) {
	auto start = std::chrono::steady_clock::now();

	next();

	auto end = std::chrono::steady_clock::now();
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	Log::info(std::format("{} {} {} {}ms", req.head.method, req.head.path, res.code, ms));
};
