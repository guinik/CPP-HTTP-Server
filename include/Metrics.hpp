#pragma once
#include <atomic>
#include <array>
#include <cstdint>
#include <string>
#include <format>

struct Metrics {
    std::atomic<uint64_t> requestsTotal    {0};
    std::atomic<uint64_t> responses2xx     {0};
    std::atomic<uint64_t> responses4xx     {0};
    std::atomic<uint64_t> responses5xx     {0};
    std::atomic<uint64_t> activeConnections{0};

    // Upper bound in ms for each finite bucket; +Inf is latencyBuckets.back().
    static constexpr std::array<uint64_t, 11> kLatencyBuckets{
        1, 5, 10, 25, 50, 100, 250, 500, 1000, 2500, 5000
    };
    // 11 finite buckets + 1 for +Inf = 12 total (all cumulative).
    std::array<std::atomic<uint64_t>, 12> latencyBuckets{};
    std::atomic<uint64_t> latencySum  {0};
    std::atomic<uint64_t> latencyCount{0};

    static_assert(kLatencyBuckets.size() + 1 == 12,
                  "latencyBuckets array size must equal kLatencyBuckets.size() + 1");

    Metrics() = default;
    Metrics(const Metrics&) = delete;
    Metrics& operator=(const Metrics&) = delete;

    void recordLatency(uint64_t ms)
    {
        latencySum  .fetch_add(ms, std::memory_order_relaxed);
        latencyCount.fetch_add(1,  std::memory_order_relaxed);
        for (size_t i = 0; i < kLatencyBuckets.size(); ++i) {
            if (ms <= kLatencyBuckets[i])
                latencyBuckets[i].fetch_add(1, std::memory_order_relaxed);
        }
        latencyBuckets.back().fetch_add(1, std::memory_order_relaxed); // +Inf
    }

    std::string snapshot() const
    {
        return std::format(
            R"({{"requests_total":{},"responses_2xx":{},"responses_4xx":{},"responses_5xx":{},"active_connections":{}}})",
            requestsTotal    .load(std::memory_order_relaxed),
            responses2xx     .load(std::memory_order_relaxed),
            responses4xx     .load(std::memory_order_relaxed),
            responses5xx     .load(std::memory_order_relaxed),
            activeConnections.load(std::memory_order_relaxed));
    }

    std::string prometheusSnapshot() const
    {
        std::string out;
        out.reserve(1024);

        out += "# HELP http_requests_total Total HTTP requests processed\n"
               "# TYPE http_requests_total counter\n";
        out += std::format("http_requests_total {}\n\n",
                           requestsTotal.load(std::memory_order_relaxed));

        out += "# HELP http_responses_total HTTP responses by status class\n"
               "# TYPE http_responses_total counter\n";
        out += std::format("http_responses_total{{status=\"2xx\"}} {}\n",
                           responses2xx.load(std::memory_order_relaxed));
        out += std::format("http_responses_total{{status=\"4xx\"}} {}\n",
                           responses4xx.load(std::memory_order_relaxed));
        out += std::format("http_responses_total{{status=\"5xx\"}} {}\n\n",
                           responses5xx.load(std::memory_order_relaxed));

        out += "# HELP http_active_connections Current active connections\n"
               "# TYPE http_active_connections gauge\n";
        out += std::format("http_active_connections {}\n\n",
                           activeConnections.load(std::memory_order_relaxed));

        out += "# HELP http_request_duration_ms HTTP request duration in milliseconds\n"
               "# TYPE http_request_duration_ms histogram\n";
        for (size_t i = 0; i < kLatencyBuckets.size(); ++i) {
            out += std::format("http_request_duration_ms_bucket{{le=\"{}\"}} {}\n",
                               kLatencyBuckets[i],
                               latencyBuckets[i].load(std::memory_order_relaxed));
        }
        out += std::format("http_request_duration_ms_bucket{{le=\"+Inf\"}} {}\n",
                           latencyBuckets.back().load(std::memory_order_relaxed));
        out += std::format("http_request_duration_ms_sum {}\n",
                           latencySum.load(std::memory_order_relaxed));
        out += std::format("http_request_duration_ms_count {}\n",
                           latencyCount.load(std::memory_order_relaxed));

        return out;
    }
};
