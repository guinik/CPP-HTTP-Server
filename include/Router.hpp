#pragma once
#include <functional>
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include <optional>
#include <memory>


using Next = std::function<void()>;
using MiddleWare = std::function<void(HTTPRequest&, HTTPResponse&, Next next)>;
using Handler = std::function<void(const HTTPRequest&, HTTPResponse&)>;

struct Route
{
	std::vector<MiddleWare> middleware;
	Handler handler;

};



struct RadixTreeNode {
	std::unordered_map<std::string, std::unique_ptr<RadixTreeNode>> children;
	std::unique_ptr<RadixTreeNode> paramChild;
	std::string paramName;
	std::optional<Route> route;

};


class RadixTree {
public:
	void add(const std::string& path, const std::string& method, const std::vector<MiddleWare>& middleware ,const Handler& handler); // "GET /users/:id" its our working idea
	std::optional<Route> match(HTTPRequest& request);
	std::unordered_map<std::string, std::unique_ptr<RadixTreeNode>> methodsRoot; //method to Root

};



void applyRoute(const std::vector<MiddleWare>& middlewareVector, HTTPRequest& request, HTTPResponse& response, Handler& handler);