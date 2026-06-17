#include "Router.hpp"
#include "StringUtils.hpp"
#include <algorithm>
#include <memory>
#include <vector>

using ParamMap = std::unordered_map<std::string, std::string>;

static constexpr DFSMode allModes[] = {
    DFSMode::DIRECT,
    DFSMode::PARAM,
    DFSMode::WILDCARD
};

// Compose a middleware vector + handler into a single callable, paid once at
// registration time.  The inner Next closure captures next_chain by reference
// to the outer lambda's by-value copy — safe because Next is always called
// synchronously within the middleware.
static std::function<void(HTTPRequest&, HTTPResponse&)>
composeChain(const std::vector<MiddleWare>& middleware, const Handler& handler)
{
    std::function<void(HTTPRequest&, HTTPResponse&)> chain =
        [handler](HTTPRequest& req, HTTPResponse& res) { handler(req, res); };

    for (int i = static_cast<int>(middleware.size()) - 1; i >= 0; --i) {
        auto next = chain;
        auto mw   = middleware[i];
        chain = [mw, next](HTTPRequest& req, HTTPResponse& res) {
            mw(req, res, [&req, &res, &next]() { next(req, res); });
        };
    }
    return chain;
}


void RouteTrie::add(const std::string& path, const std::string& method,
                    const std::vector<MiddleWare>& middleware, const Handler& handler)
{
    std::vector<std::string> splittedVector = splitByDelimiter(path, "/");

    if (!methodsRoot) {
        methodsRoot = std::make_unique<RouteTrieNode>();
    }

    RouteTrieNode* currentNodePtr = methodsRoot.get();

    for (size_t i = 0; i < splittedVector.size(); i++)
    {
        std::string currStr = splittedVector[i];
        if (currStr.empty()) continue;
        if (currStr[0] == ':')
        {
            currStr = currStr.substr(1, currStr.length() - 1);
            if (!currentNodePtr->paramChild)
            {
                currentNodePtr->paramChild = std::make_unique<RouteTrieNode>();
                currentNodePtr->paramChild->paramName = currStr;
            }
            currentNodePtr = currentNodePtr->paramChild.get();
            continue;
        }
        else if (currStr[0] == '*')
        {
            if (!currentNodePtr->wildcardChild)
            {
                currentNodePtr->wildcardChild = std::make_unique<RouteTrieNode>();
            }
            currentNodePtr = currentNodePtr->wildcardChild.get();
            continue;
        }

        auto it = currentNodePtr->children.find(currStr);
        if (it != currentNodePtr->children.end())
        {
            currentNodePtr = it->second.get();
        }
        else
        {
            (currentNodePtr->children)[currStr] = std::make_unique<RouteTrieNode>();
            currentNodePtr = (currentNodePtr->children)[currStr].get();
        }
    }

    if (currentNodePtr->routeMap.count(method))
        throw std::runtime_error("Already registered: " + method + " " + path);

    currentNodePtr->routeMap[method] = Route{ .composedChain = composeChain(middleware, handler) };
}


static std::pair<const RouteTrieNode*, ParamMap>
dfsFindMatch(const RouteTrieNode* initialNode, const std::vector<std::string>& pathVector,
             const DFSMode& mode, size_t currentIndex, ParamMap paramMap)
{
    if (currentIndex == pathVector.size())
        return { initialNode, paramMap };

    const RouteTrieNode* currentNode = initialNode;
    switch (mode)
    {
        case DFSMode::DIRECT:
        {
            auto it = currentNode->children.find(pathVector[currentIndex]);
            if (it != currentNode->children.end())
                currentNode = it->second.get();
            else
                return { nullptr, paramMap };
            break;
        }
        case DFSMode::PARAM:
            if (currentNode->paramChild)
            {
                currentNode = currentNode->paramChild.get();
                paramMap[currentNode->paramName] = pathVector[currentIndex];
            }
            else
                return { nullptr, paramMap };
            break;

        case DFSMode::WILDCARD:
        {
            if (currentNode->wildcardChild)
            {
                std::string wildCardString;
                for (size_t i = currentIndex; i < pathVector.size(); i++)
                {
                    if (i > currentIndex) wildCardString += "/";
                    wildCardString += pathVector[i];
                }
                paramMap["*"] = wildCardString;
                return { currentNode->wildcardChild.get(), paramMap };
            }
            else
                return { nullptr, paramMap };
        }
        default:
            return { currentNode, paramMap };
    }

    for (auto& nextMode : allModes)
    {
        auto [dfsResultPtr, paramResult] = dfsFindMatch(currentNode, pathVector, nextMode, currentIndex + 1, paramMap);
        if (dfsResultPtr)
            return { dfsResultPtr, paramResult };
    }
    return { nullptr, paramMap };
}


RouteMatch RouteTrie::match(const HTTPRequest& request) const
{
    if (!methodsRoot) return {};

    ParamMap queryParams;
    std::vector<std::string> pathAndQueryVector = splitByDelimiter(request.head.path, "?");

    if (pathAndQueryVector.size() > 1)
    {
        std::vector<std::string> extraParams = splitByDelimiter(pathAndQueryVector[1], "&");
        for (auto& param : extraParams)
        {
            std::string decodedString = stringDecode(param);
            std::vector<std::string> splitByEqual = splitByDelimiter(decodedString, "=");
            if (splitByEqual.size() == 2)
                queryParams[splitByEqual[0]] = splitByEqual[1];
        }
    }

    std::string fullPath = pathAndQueryVector[0];
    std::vector<std::string> splittedPath = splitByDelimiter(fullPath, "/");
    splittedPath.erase(std::remove(splittedPath.begin(), splittedPath.end(), ""), splittedPath.end());

    ParamMap emptyMap{};
    for (auto& mode : allModes)
    {
        auto [resultNodePtrDfs, paramResult] = dfsFindMatch(methodsRoot.get(), splittedPath, mode, 0, emptyMap);
        if (resultNodePtrDfs)
        {
            auto it = resultNodePtrDfs->routeMap.find(request.head.method);
            if (it != resultNodePtrDfs->routeMap.end())
            {
                return RouteMatch{
                    .route          = &it->second,
                    .pathFound      = true,
                    .allowedMethods = {},
                    .params         = std::move(paramResult),
                    .queryParams    = std::move(queryParams),
                };
            }

            std::vector<std::string> allowed;
            for (const auto& [method, _] : resultNodePtrDfs->routeMap)
                allowed.push_back(method);
            return RouteMatch{
                .route          = nullptr,
                .pathFound      = true,
                .allowedMethods = std::move(allowed),
                .params         = {},
                .queryParams    = {},
            };
        }
    }
    return {};
}
