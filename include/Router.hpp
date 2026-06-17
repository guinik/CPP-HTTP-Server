#pragma once
#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include "IRouter.hpp"
#include "HTTPErrors.hpp"
#include "StringUtils.hpp"

enum class DFSMode { DIRECT, PARAM, WILDCARD };

struct RouteTrieNode {
    std::unordered_map<std::string, std::unique_ptr<RouteTrieNode>> children;
    std::unique_ptr<RouteTrieNode> paramChild;
    std::unique_ptr<RouteTrieNode> wildcardChild;
    std::string paramName;
    std::unordered_map<std::string, Route> routeMap;
};

class RouteTrie : public IRouter {
public:
    void add(const std::string& path, const std::string& method,
             const std::vector<MiddleWare>& middleware, const Handler& handler);
    RouteMatch match(const HTTPRequest& request) const override;

private:
    std::unique_ptr<RouteTrieNode> methodsRoot;
};
