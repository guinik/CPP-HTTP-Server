#include "ThreadPool.hpp"
#include "Logger.hpp"
#include <format>

ThreadPool::ThreadPool(size_t numThreads, size_t maxQueueDepth_)
    : maxQueueDepth(maxQueueDepth_) {

	for (size_t i{}; i < numThreads; i++) {
		threads.emplace_back([this]() {

			while (true) {
				Task task;
				{
					std::unique_lock<std::mutex> lock(queueMtx);
					cv.wait(lock, [this]() {
						return !tasks.empty() || stop;
						}
					);
					if (stop && tasks.empty()) 
					{
						return;
					}
					task = std::move(tasks.front());
					tasks.pop();
				}
				try {
					task();
				} catch (const std::exception& e) {
					Log::error(std::format("[ThreadPool] task threw: {}", e.what()));
				} catch (...) {
					Log::error("[ThreadPool] task threw unknown exception");
				}
			}
			}
		);
	};
}

ThreadPool::~ThreadPool() {

	{
		std::unique_lock lock(queueMtx);
		stop = true;
	}
	cv.notify_all();
	for( auto& t: threads)
	{
		t.join();
	}
}