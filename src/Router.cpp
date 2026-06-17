#include "Router.hpp"
#include "StringUtils.hpp"
#include <optional>
#include <memory>
#include <vector>

static constexpr DFSMode allModes[] = {
	DFSMode::DIRECT,
	DFSMode::PARAM,
	DFSMode::WILDCARD
};

static bool isHexDigit(char c)
{
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

std::string stringDecode(std::string input)
{
	std::string result;
	for(size_t i{}; i < input.size(); i++)
	{
		if (input[i] == '%' && i + 2 < input.size())
		{
			if (!isHexDigit(input[i + 1]) || !isHexDigit(input[i + 2]))
				throw BadRequestException("Invalid percent-encoding in URL");
			int hexValue = std::stoi(input.substr(i + 1, 2), nullptr, 16);
			result += static_cast<char>(hexValue);
			i += 2;
		}
		else if(input[i] == '+')
		{
			result += ' ';
		}
		else
		{
			result += input[i];
		}
	}
	return result;
}


void RouteTrie::add(const std::string& path, const std::string& method, const std::vector<MiddleWare>& middleware, const Handler& handler)
{
	std::vector<std::string> splittedVector = splitByDelimiter(path, "/");

	if (!methodsRoot) {
		methodsRoot = std::make_unique<RouteTrieNode>();
	};

	RouteTrieNode* currentNodePtr = methodsRoot.get();

	for (size_t i = 0; i < splittedVector.size(); i++)
	{
		std::string currStr = splittedVector[i];
		if(currStr[0] == ':')
		{
			currStr = currStr.substr(1, currStr.length() - 1);
			if(!currentNodePtr->paramChild)
			{
				currentNodePtr->paramChild = std::make_unique<RouteTrieNode>();
				currentNodePtr->paramChild->paramName = currStr;
			}
			currentNodePtr = currentNodePtr -> paramChild.get();
			continue;
		}
		else if(currStr[0] == '*') 
		{
			if (!currentNodePtr->wildcardChild) 
			{
				currentNodePtr->wildcardChild = std::make_unique<RouteTrieNode>();
			}
			currentNodePtr = currentNodePtr->wildcardChild.get();
			continue;
		
		}
		
		auto it = currentNodePtr->children.find(currStr);
		if(it != currentNodePtr->children.end())
		{
			currentNodePtr = it->second.get();
		}
		else 
		{
			(currentNodePtr->children)[currStr] = std::make_unique<RouteTrieNode>();
			currentNodePtr = (currentNodePtr->children)[currStr].get();
		}

	};
	
	auto it = currentNodePtr->routeMap.find(method);
	if (it != currentNodePtr->routeMap.end()) 
	{
		throw std::runtime_error("Already registered");
	}
	else 
	{

		currentNodePtr->routeMap[method] = Route{
									.middleware = middleware,
									.handler = handler
								};

	}
};


void mergeParams(HTTPRequest& request, const CaseInsensitiveMap& params)
{
	for (auto [key, value] : params)
	{
		request.head.params[key] = value;
	}
}




std::pair<RouteTrieNode*, CaseInsensitiveMap> dfsFindMatch(RouteTrieNode* initialNode, 
	const std::vector<std::string>& pathVector, const DFSMode& mode, size_t currentIndex,
			CaseInsensitiveMap paramMap)
{
	if (currentIndex == pathVector.size())
	{
		return { initialNode, paramMap };
	};
	RouteTrieNode* currentNode = initialNode;
	switch (mode)
	{
		case DFSMode::DIRECT:
		{

			auto it = currentNode->children.find(pathVector[currentIndex]);

			if (it != currentNode->children.end())
			{
				currentNode = it->second.get();

			}
			else
			{
				return { nullptr, paramMap };
			}
			break;
		}

		case DFSMode::PARAM:

			if (currentNode->paramChild)
			{
				currentNode = currentNode->paramChild.get();
				paramMap[currentNode->paramName] = pathVector[currentIndex];
			}
			else 
			{
				return { nullptr, paramMap };
			}


			break;

		case DFSMode::WILDCARD:
		{

			if (currentNode->wildcardChild)
			{
				std::string wildCardString;
				for (size_t i = currentIndex; i < pathVector.size(); i++)
				{
					if (i > currentIndex)
					{
						wildCardString += "/";
					}
					wildCardString += pathVector[i];
				}
				paramMap["*"] = wildCardString;
				currentIndex = pathVector.size();
				return { currentNode->wildcardChild.get(), paramMap };
			}
			else 
			{
				return { nullptr, paramMap };
			}
			break;
		}

		default:
			return { currentNode, paramMap };
	}
	for (auto& mode : allModes)
	{
		auto [dfsResultPtr, paramResult] = dfsFindMatch(currentNode, pathVector, mode, currentIndex + 1, paramMap);
		if (dfsResultPtr)
		{
			return { dfsResultPtr, paramResult };
		}

	}

	return { nullptr, paramMap };
	
	
};

RouteMatch RouteTrie::match(HTTPRequest& requestWithBody) {
	auto& requestHead = requestWithBody.head;
	if (!methodsRoot) {
		return {};
	};
	RouteTrieNode* methodHeadPtr = methodsRoot.get();

	std::vector<std::string> pathAndQueryVector = splitByDelimiter(requestHead.path, "?");

	if(pathAndQueryVector.size() > 1)
	{
		std::vector<std::string> extraParams = splitByDelimiter(pathAndQueryVector[1], "&");
		for(auto& param : extraParams)
		{
			std::string decodedString = stringDecode(param);
			std::vector<std::string> splitByEqual = splitByDelimiter(decodedString, "=");
			if(splitByEqual.size() == 2)
			{
				requestHead.queryParams[splitByEqual[0]] = splitByEqual[1];
			}
		}
	}

	std::string fullPath = pathAndQueryVector[0];
	std::vector<std::string> splittedPath = splitByDelimiter(fullPath, "/");

	size_t initialIndex = 0;
	CaseInsensitiveMap emptyMap{};
	for (auto& mode : allModes)
	{
		auto [resultNodePtrDfs, paramResult] = dfsFindMatch(methodHeadPtr, splittedPath, mode, initialIndex, emptyMap);
		if (resultNodePtrDfs)
		{
			mergeParams(requestWithBody, paramResult);
			auto it = resultNodePtrDfs->routeMap.find(requestWithBody.head.method);
			if (it != resultNodePtrDfs->routeMap.end())
			{
				return { &it->second, true };
			}
			return { nullptr, true };
		}

	}
	return {};

};



void applyRoute(const std::vector<MiddleWare>& middlewareVector, HTTPRequest& request, HTTPResponse& response, Handler& handler)
{
	std::function<void()> chain = [&]() {
		handler(request, response);
	};

	for (int i = middlewareVector.size() - 1; i >= 0; --i) {
		auto next = chain;
		auto currentFunction = middlewareVector[i];
		chain = [currentFunction, &request, &response, next]() {
			currentFunction(request, response, next);
		};
	}

	chain();
}

