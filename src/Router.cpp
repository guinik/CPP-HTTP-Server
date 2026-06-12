#include "Router.hpp"
#include <optional>
#include <memory>
#include "HTTPRequest.hpp"

void RadixTree::add(const std::string& path, const std::string& method, const Handler& handler)
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
	
	currentNodePtr->handler = std::optional<Handler>(handler);

};

std::optional<Handler> RadixTree::match(HTTPRequest& requestWithBody) {

	auto& requestHead = requestWithBody.head;
	if (!methodsRoot.count(requestHead.method)) {
		return std::nullopt;
	};
	RadixTreeNode* currentNodePtr = methodsRoot[requestHead.method].get();

	std::vector<std::string> splittedPath = splitByDelimiter(requestHead.path, "/");

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

	return currentNodePtr->handler;
};
