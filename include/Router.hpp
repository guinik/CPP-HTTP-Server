#pragma once
#include <functional>
#include <memory>
#include <stdexcept>
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"

class BadRequestException : public std::runtime_error {
public:
	explicit BadRequestException(const std::string& msg) : std::runtime_error(msg) {}
};


using Next = std::function<void()>;
using MiddleWare = std::function<void(HTTPRequest&, HTTPResponse&, Next next)>;
using Handler = std::function<void(const HTTPRequest&, HTTPResponse&)>;

struct Route
{
	std::vector<MiddleWare> middleware;
	Handler handler;

};


enum class DFSMode
{
	DIRECT,
	PARAM,
	WILDCARD
};


struct RadixTreeNode {
	std::unordered_map<std::string, std::unique_ptr<RadixTreeNode>> children;
	std::unique_ptr<RadixTreeNode> paramChild;
	std::unique_ptr<RadixTreeNode> wildcardChild;
	std::string paramName;
	std::unordered_map<std::string, Route> routeMap;

};


struct RouteMatch {
	Route* route = nullptr;
	bool pathFound = false;
};

class RadixTree {
public:
	void add(const std::string& path, const std::string& method, const std::vector<MiddleWare>& middleware, const Handler& handler);
	RouteMatch match(HTTPRequest& request);

private:
	std::unique_ptr<RadixTreeNode> methodsRoot;
};



void applyRoute(const std::vector<MiddleWare>& middlewareVector, HTTPRequest& request, HTTPResponse& response, Handler& handler);

std::string stringDecode(std::string input);