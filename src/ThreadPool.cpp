#include "ThreadPool.hpp"

void ThreadPool::enqueue(std::function<void()> task)
{
	{
		std::unique_lock<std::mutex> lock(queueMtx);
		tasks.push(std::move(task));
	}
	cv.notify_one();
}


ThreadPool::ThreadPool(size_t numThreads) {

	for (size_t i{}; i < numThreads; i++) {
		threads.emplace_back([this]() {

			while (true) {
				std::function<void()> task;
				{
					std::unique_lock<std::mutex> lock(queueMtx);
					cv.wait(lock, [this]() {
						return !tasks.empty() || stop;
						}
					);
					if (stop && tasks.empty()) return;
					task = std::move(tasks.front());
					tasks.pop();
				}
				task();
			}
			}
		);
	};
}

ThreadPool::~ThreadPool() {

	{
		std::unique_lock lock(queueMtx);
		stop = false;
	}
	cv.notify_all();
	for( auto& t: threads)
	{
		t.join();
	}
}