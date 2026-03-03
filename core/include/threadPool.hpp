/**
 * Copyright (c) 2026 Gilang Teja Krishna
 * github.com/gtkrshnaaa
 *
 * ThreadPool.hpp - Nevaarize Concurrency Runtime
 *
 * Fixed-size work-stealing thread pool for async/await.
 * Replaces per-task std::thread spawning with pooled execution.
 */

#ifndef NEVAARIZE_THREAD_POOL_HPP
#define NEVAARIZE_THREAD_POOL_HPP

#include <cstdint>
#include <functional>
#include <future>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <vector>
#include <atomic>

namespace nevaarize {

/**
 * Fixed-size thread pool for JIT async task execution.
 * Uses a shared task queue with mutex-based synchronization.
 */
class ThreadPool {
public:
    /**
     * Create thread pool with hardware_concurrency() workers.
     */
    static ThreadPool& instance() {
        static ThreadPool pool;
        return pool;
    }

    /**
     * Submit a task returning int64_t for execution.
     * Returns a shared_future that can be awaited.
     */
    std::shared_future<int64_t> submit(std::function<int64_t()> task) {
        auto promise = std::make_shared<std::promise<int64_t>>();
        std::shared_future<int64_t> future = promise->get_future();

        {
            std::lock_guard<std::mutex> lock(queueMutex);
            tasks.emplace([promise, task = std::move(task)]() {
                try {
                    int64_t result = task();
                    promise->set_value(result);
                } catch (...) {
                    promise->set_exception(std::current_exception());
                }
            });
        }

        condition.notify_one();
        return future;
    }

    /**
     * Get number of worker threads.
     */
    size_t workerCount() const { return workers.size(); }

    /**
     * Get number of pending tasks.
     */
    size_t pendingTasks() const {
        std::lock_guard<std::mutex> lock(queueMutex);
        return tasks.size();
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            stopping = true;
        }
        condition.notify_all();
        for (auto& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

private:
    ThreadPool() : stopping(false) {
        unsigned int numThreads = std::thread::hardware_concurrency();
        if (numThreads == 0) numThreads = 4;

        workers.reserve(numThreads);
        for (unsigned int i = 0; i < numThreads; ++i) {
            workers.emplace_back([this]() {
                workerLoop();
            });
        }
    }

    void workerLoop() {
        while (true) {
            std::function<void()> task;
            {
                std::unique_lock<std::mutex> lock(queueMutex);
                condition.wait(lock, [this]() {
                    return stopping || !tasks.empty();
                });

                if (stopping && tasks.empty()) return;

                task = std::move(tasks.front());
                tasks.pop();
            }
            task();
        }
    }

    std::vector<std::thread> workers;
    std::queue<std::function<void()>> tasks;
    mutable std::mutex queueMutex;
    std::condition_variable condition;
    bool stopping;
};

} // namespace nevaarize

#endif // NEVAARIZE_THREAD_POOL_HPP
