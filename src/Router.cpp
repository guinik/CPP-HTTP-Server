#include "Router.hpp"
#include <optional>
#include <memory>
#include "HTTPRequest.hpp"

void RadixTree::add(const std::string& path, const std::string& method, const std::vector<MiddleWare>& middleware, const Handler& handler)
{
	// first we will split by / 
	std::vector<std::string> splittedVector = splitByDelimiter(path, "/");

	if (!methodsRoot.count(method)) {
		methodsRoot[method] = std::make_unique<RadixTreeNode>();
	};

	RadixTreeNode* currentNodePtr = methodsRoot[method].get();

	for (int i = 0; i < splittedVector.size(); i++)
	{
		std::string currStr = splittedVector[i];
		if(currStr[0] == ':')
		{
			currStr = currStr.substr(1, currStr.length() - 1);
			if(!currentNodePtr->paramChild)
			{
				currentNodePtr->paramChild = std::make_unique<RadixTreeNode>();
				currentNodePtr->paramChild->paramName = currStr;
			}
			currentNodePtr = currentNodePtr -> paramChild.get();
			continue;
		}
		
		auto it = currentNodePtr->children.find(currStr);
		if(it != currentNodePtr->children.end())
		{
			currentNodePtr = it->second.get();
		}
		else 
		{
			(currentNodePtr->children)[currStr] = std::make_unique<RadixTreeNode>();
			currentNodePtr = (currentNodePtr->children)[currStr].get();
		}

	};
	

	currentNodePtr->route =Route{
								.middleware = middleware,
								.handler = handler
							};

};

std::optional<Route> RadixTree::match(HTTPRequest& requestWithBody) {
	// /users?id=100/... 
	auto& requestHead = requestWithBody.head;
	if (!methodsRoot.count(requestHead.method)) {
		return std::nullopt;
	};
	RadixTreeNode* currentNodePtr = methodsRoot[requestHead.method].get();

	std::vector<std::string> pathAndQueryVector = splitByDelimiter(requestHead.path, "?");

	if(pathAndQueryVector.size() > 1)
	{
		std::vector<std::string> extraParams = splitByDelimiter(pathAndQueryVector[1], "&"); // id=100;
		for(auto& param : extraParams)
		{
			std::vector<std::string> splitByEqual = splitByDelimiter(param, "=");
			requestHead.queryParams[splitByEqual[0]] = splitByEqual[1];
		}
	}

	std::string fullPath = pathAndQueryVector[0];
	std::vector<std::string> splittedPath = splitByDelimiter(fullPath, "/");



	for (size_t i = 0; i < splittedPath.size(); i++)
	{

		std::string currentString = splittedPath[i];


		auto it = currentNodePtr->children.find(currentString);
		if (it != currentNodePtr->children.end())
		{
			currentNodePtr = it->second.get();
			continue;
		}

		if (currentNodePtr->paramChild)
		{
			currentNodePtr = currentNodePtr->paramChild.get();
			requestHead.params[currentNodePtr->paramName] = currentString;
			continue;

		}

		return std::nullopt;
	}

	return currentNodePtr->route;
};




void applyRoute(const std::vector<MiddleWare>& middlewareVector, HTTPRequest& request, HTTPResponse& response, Handler& handler)
{
	std::function<void()> chain = [](){};

	for (int i = middlewareVector.size() - 1; i >= 0; --i) {

		auto next = chain;
		auto& currentFunction = middlewareVector[i];

		chain = [&currentFunction, &request, &response, next]() {
			currentFunction(request, response, next);
		};
	};

	chain();
	if (response.code.empty()) {
		handler(request, response);
	}


}