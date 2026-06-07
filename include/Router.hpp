#pragma once
#include <functional>
#include "HTTPRequest.hpp"
#include "HTTPResponse.hpp"
#include <optional>
#include <memory>

using Handler = std::function<HTTPResponse(const HTTPRequest&)>;


struct RadixTreeNode {
	std::unordered_map<std::string, std::unique_ptr<RadixTreeNode>> children;
	std::unique_ptr<RadixTreeNode> paramChild;
	std::string paramName;
	std::optional<Handler> handler;

};


class RadixTree {
public:
	void add(const std::string& path, const std::string& method, const Handler& handler); // "GET /users/:id" its our working idea
	std::optional<Handler> match(HTTPRequest& request);
	std::unordered_map<std::string, std::unique_ptr<RadixTreeNode>> methodsRoot; //method to Root

};