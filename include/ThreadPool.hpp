#pragma once
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <functional>
class ThreadPool {
public:
	ThreadPool(size_t numThreads);
	~ThreadPool();
	void enqueue(std::function<void()> task);

private:
	std::vector<std::thread> threads;
	std::mutex queueMtx;
	std::queue<std::function<void()>> tasks;
	std::condition_variable cv;
	bool stop = false;

};