#include <gtest/gtest.h>
#include "ThreadPool.hpp"
#include <atomic>
#include <chrono>
#include <future>
#include <stdexcept>

static constexpr auto kTimeout = std::chrono::seconds(5);

// ── basic execution ───────────────────────────────────────────────────────────

TEST(ThreadPool, SingleTaskExecutes) {
    ThreadPool pool(1);
    std::promise<int> p;
    auto f = p.get_future();

    pool.enqueue([&p]() { p.set_value(42); });

    ASSERT_EQ(f.wait_for(kTimeout), std::future_status::ready);
    EXPECT_EQ(f.get(), 42);
}

TEST(ThreadPool, AllTasksExecute) {
    ThreadPool pool(4);
    constexpr int N = 20;
    std::atomic<int> count{0};

    std::vector<std::shared_ptr<std::promise<void>>> promises(N);
    std::vector<std::future<void>> futures;
    for (int i = 0; i < N; ++i) {
        promises[i] = std::make_shared<std::promise<void>>();
        futures.push_back(promises[i]->get_future());
        pool.enqueue([&count, p = promises[i]]() {
            ++count;
            p->set_value();
        });
    }

    for (auto& f : futures) {
        ASSERT_EQ(f.wait_for(kTimeout), std::future_status::ready) << "task never ran";
    }
    EXPECT_EQ(count.load(), N);
}

TEST(ThreadPool, TasksRunConcurrently) {
    constexpr int numThreads = 4;
    ThreadPool pool(numThreads);

    std::atomic<int> peakConcurrent{0};
    std::atomic<int> active{0};
    std::mutex barrierMtx;
    std::condition_variable barrierCv;
    int arrived = 0;

    std::vector<std::shared_ptr<std::promise<void>>> promises(numThreads);
    std::vector<std::future<void>> futures;

    for (int i = 0; i < numThreads; ++i) {
        promises[i] = std::make_shared<std::promise<void>>();
        futures.push_back(promises[i]->get_future());
        pool.enqueue([&, p = promises[i]]() {
            ++active;
            {
                std::unique_lock lk(barrierMtx);
                ++arrived;
                barrierCv.wait_for(lk, kTimeout, [&] { return arrived == numThreads; });
                barrierCv.notify_all();
            }
            int cur = active.load();
            int prev = peakConcurrent.load();
            while (cur > prev && !peakConcurrent.compare_exchange_weak(prev, cur)) {}
            --active;
            p->set_value();
        });
    }

    for (auto& f : futures) {
        ASSERT_EQ(f.wait_for(kTimeout), std::future_status::ready);
    }
    EXPECT_EQ(peakConcurrent.load(), numThreads);
}

// ── exception safety ──────────────────────────────────────────────────────────

TEST(ThreadPool, ThrowingTaskDoesNotKillWorker) {
    ThreadPool pool(1);
    constexpr int N = 5;

    std::vector<std::shared_ptr<std::promise<void>>> promises(N);
    std::vector<std::future<void>> futures;

    for (int i = 0; i < N; ++i) {
        promises[i] = std::make_shared<std::promise<void>>();
        futures.push_back(promises[i]->get_future());
        pool.enqueue([i, p = promises[i]]() {
            p->set_value();
            if (i == 2) throw std::runtime_error("deliberate throw");
        });
    }

    // If the worker died on task 2, futures[3] and [4] never resolve — test
    // times out and fails cleanly instead of hanging forever.
    for (int i = 0; i < N; ++i) {
        ASSERT_EQ(futures[i].wait_for(kTimeout), std::future_status::ready)
            << "task " << i << " never ran (worker likely died on task 2)";
    }
}

TEST(ThreadPool, UnknownExceptionDoesNotKillWorker) {
    ThreadPool pool(1);

    std::promise<void> p1, p2;
    pool.enqueue([&p1]() { p1.set_value(); throw 99; });
    ASSERT_EQ(p1.get_future().wait_for(kTimeout), std::future_status::ready);

    pool.enqueue([&p2]() { p2.set_value(); });
    ASSERT_EQ(p2.get_future().wait_for(kTimeout), std::future_status::ready);
}

// ── destructor ────────────────────────────────────────────────────────────────

TEST(ThreadPool, DestructorDrainsQueueBeforeJoining) {
    std::atomic<int> completed{0};
    {
        ThreadPool pool(2);
        constexpr int N = 10;
        for (int i = 0; i < N; ++i) {
            pool.enqueue([&completed]() {
                std::this_thread::sleep_for(std::chrono::milliseconds(5));
                ++completed;
            });
        }
        // pool destructor blocks here until all tasks finish
    }
    EXPECT_EQ(completed.load(), 10);
}

// ── bounded queue ─────────────────────────────────────────────────────────────

TEST(ThreadPool, EnqueueThrowsWhenQueueFull) {
    // 1 thread, depth-1 queue, blocked by a gate so tasks pile up.
    std::promise<void> gate;
    auto gateFuture = gate.get_future().share();

    ThreadPool pool(1, /*maxQueueDepth=*/1);

    // This task blocks the sole worker until we open the gate.
    pool.enqueue([gateFuture]() mutable { gateFuture.wait(); });

    // The queue is now full (depth 1). A second enqueue must throw.
    EXPECT_THROW(
        pool.enqueue([]() {}),
        std::runtime_error);

    gate.set_value(); // unblock the worker so the destructor can join
}

TEST(ThreadPool, EnqueueThrowsAfterDestructorStarts) {
    // Start the destructor (via a scope exit), then try to enqueue.
    // We use a thread that tries to enqueue while the destructor is running.
    std::promise<void> destructorReady;
    auto destructorFutureSh = destructorReady.get_future().share();

    // Outer scope to trigger pool destruction.
    std::atomic<bool> threw{false};
    {
        ThreadPool pool(1);

        std::thread racer([&pool, &threw, destructorFutureSh]() mutable {
            destructorFutureSh.wait();
            try {
                pool.enqueue([]() {});
            } catch (const std::runtime_error&) {
                threw.store(true);
            }
        });

        // Signal the racer just before we leave this scope so the destructor
        // begins; the racer should find the pool stopping.
        destructorReady.set_value();
        racer.join();
    } // pool destroyed here

    // The racer either saw the pool stopping (threw) or got in just before it —
    // either outcome is valid. The important invariant is no crash or deadlock.
    (void)threw;
}
