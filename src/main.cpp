#include "HttpServer.hpp"
#include "Router.hpp"
#include "UserRoutes.hpp"
#include "ThreadPool.hpp"
#include "Logger.hpp"
#include "Metrics.hpp"
#include "ServerConfig.hpp"
#include <thread>
#include <string>
#include <format>
#include <csignal>
#include <cstdlib>
#include <optional>

static std::optional<std::string> readEnv(const char* name)
{
#ifdef _WIN32
    size_t len = 0;
    getenv_s(&len, nullptr, 0, name);
    if (len == 0) return std::nullopt;
    std::string val(len, '\0');
    getenv_s(&len, val.data(), len, name);
    val.resize(len > 0 ? len - 1 : 0);
    return val;
#else
    const char* v = std::getenv(name);
    return v ? std::optional<std::string>{v} : std::nullopt;
#endif
}

static_assert(std::atomic<bool>::is_always_lock_free,
    "atomic_bool must be lock-free to be used safely in a signal handler");

static std::atomic_bool g_running{ true };

void handleShutdown(int)
{
    g_running.store(false, std::memory_order_relaxed);
}

static size_t parsePositiveSize(const char* s)
{
    try {
        long long v = std::stoll(s);
        return (v > 0) ? static_cast<size_t>(v) : 0;
    } catch (...) {
        return 0;
    }
}

static void applyEnvOverrides(ServerConfig& cfg)
{
    if (auto v = readEnv("HTTP_PORT")) {
        size_t n = parsePositiveSize(v->c_str());
        if (n >= 1 && n <= 65535) cfg.port = *v;
        else Log::error(std::format("Ignoring invalid HTTP_PORT='{}'; keeping '{}'.", *v, cfg.port));
    }
    if (auto v = readEnv("HTTP_BIND"))
        cfg.bindAddress = *v;
    if (auto v = readEnv("HTTP_THREADS")) {
        size_t n = parsePositiveSize(v->c_str());
        if (n > 0) cfg.threadCount = n;
        else Log::error(std::format("Ignoring invalid HTTP_THREADS='{}'.", *v));
    }
    if (auto v = readEnv("HTTP_MAX_CONN")) {
        size_t n = parsePositiveSize(v->c_str());
        if (n > 0) cfg.maxConnections = n;
        else Log::error(std::format("Ignoring invalid HTTP_MAX_CONN='{}'.", *v));
    }
    if (auto v = readEnv("HTTP_QUEUE_DEPTH")) {
        size_t n = parsePositiveSize(v->c_str());
        if (n > 0) cfg.maxQueueDepth = n;
        else Log::error(std::format("Ignoring invalid HTTP_QUEUE_DEPTH='{}'.", *v));
    }
    if (auto v = readEnv("HTTP_LOG_LEVEL")) {
        if      (*v == "debug") cfg.logLevel = Log::Level::debug;
        else if (*v == "info")  cfg.logLevel = Log::Level::info;
        else if (*v == "warn")  cfg.logLevel = Log::Level::warn;
        else if (*v == "error") cfg.logLevel = Log::Level::error;
        else Log::warn(std::format("Ignoring unknown HTTP_LOG_LEVEL='{}'; keeping '{}'.", *v,
                        cfg.logLevel == Log::Level::debug ? "debug" :
                        cfg.logLevel == Log::Level::info  ? "info"  :
                        cfg.logLevel == Log::Level::warn  ? "warn"  : "error"));
    }
}

int main(int argc, char* argv[]) {
    std::signal(SIGINT, handleShutdown);
    try {
        ServerConfig config;

        if (argc > 1) {
            size_t n = parsePositiveSize(argv[1]);
            if (n >= 1 && n <= 65535)
                config.port = argv[1];
            else
                Log::error(std::format("Invalid port '{}'; using default {}.", argv[1], config.port));
        }

        applyEnvOverrides(config);
        Log::setLevel(config.logLevel);

        Log::info(std::format("Starting: bind={}:{} threads={} max_conn={}",
                  config.bindAddress, config.port,
                  config.threadCount, config.maxConnections));

        Metrics metrics;
        RouteTrie router;
        addUserRoutes(router, metrics);

        ThreadPool threadPool(config.threadCount, config.maxQueueDepth);

        HttpServer server(router, threadPool, g_running, metrics, config);
        server.run();
    }
    catch (const std::exception& e) {
        Log::error(std::format("Fatal error: {}", e.what()));
        return 1;
    }

    return 0;
}
