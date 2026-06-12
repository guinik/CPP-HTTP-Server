#pragma once
#include "Router.hpp"
#include "json.hpp"

using json = nlohmann::json;
MiddleWare parseJson = [](HTTPRequest& req, HTTPResponse& res, Next next) {
	if (req.head.headers["Content-Type"] != "application/json") 
	{
		res.code = 415;
		res.reason = "Unsported media no json";
		res.version = "HTTP/1.1";
	};
	try {
		req.body.json = json::parse(req.body.raw);
		next();
	}
	catch (json::parse_error& e ){
		res.code = 400;
		res.reason = "Invalid Json";
		res.version = "HTTP/1.1";
	}


};

