#pragma once
#include <thread>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>

class Task {
public:
	template <typename F>
	Task(F&& f)
		: func_(std::make_unique<Model<std::decay_t<F>>>(std::forward<F>(f))) {}
	Task() = default;
	Task(Task&&) noexcept = default;
	Task& operator=(Task&&) noexcept = default;
	Task(const Task&) = delete;
	Task& operator=(const Task&) = delete;
	void operator()() { func_->call(); }

private:
	struct Concept {
		virtual ~Concept() = default;
		virtual void call() = 0;
	};
	template <typename F>
	struct Model : Concept {
		F f;
		Model(F&& f_) : f(std::move(f_)) {}
		void call() override { f(); }
	};
	std::unique_ptr<Concept> func_;
};

class ThreadPool {
public:
	explicit ThreadPool(size_t numThreads, size_t maxQueueDepth = 4096);
	~ThreadPool();

	// Throws std::runtime_error if the pool is stopping or the queue is full.
	template <typename F>
	void enqueue(F&& f) {
		{
			std::unique_lock<std::mutex> lock(queueMtx);
			if (stop)
				throw std::runtime_error("ThreadPool is stopping");
			if (tasks.size() >= maxQueueDepth)
				throw std::runtime_error("ThreadPool queue full");
			tasks.emplace(std::forward<F>(f));
		}
		cv.notify_one();
	}

private:
	std::vector<std::thread> threads;
	std::mutex queueMtx;
	std::queue<Task> tasks;
	std::condition_variable cv;
	size_t maxQueueDepth;
	bool stop = false;
};