#pragma once
#include "Router.hpp"
#include "json.hpp"
#include <iostream>

using json = nlohmann::json;

MiddleWare parseJson = [](HTTPRequest& req, HTTPResponse& res, Next next) {

	if (req.body.raw.empty())
	{
		next();
		return;
	}
	if (req.head.headers.count("Content-Type") && req.head.headers["Content-Type"] != "application/json")
	{
		res.code = "415";
		res.reason = "Unsported media no json";
		res.version = "HTTP/1.1";
		return;
	};
	try {
		req.body.json = json::parse(req.body.raw);
		next();
		return;
	}
	catch (json::parse_error& e ){
		res.code = "400";
		res.reason = "Invalid Json";
		res.version = "HTTP/1.1";
	}
};



MiddleWare requestLogger = [](HTTPRequest& req, HTTPResponse& res, Next next) {
	auto start = std::chrono::steady_clock::now();

	next();

	auto end = std::chrono::steady_clock::now();
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

	std::cout << req.head.method << " " << req.head.path
		<< " " << res.code
		<< " " << ms << "ms\n";
	};
