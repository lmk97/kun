#ifndef KUN_LOOP_THREAD_POOL_H
#define KUN_LOOP_THREAD_POOL_H

#include <thread>
#include <vector>

namespace kun {

class AsyncHandler;

class ThreadPool {
public:
    ThreadPool(const ThreadPool&) = delete;

    ThreadPool& operator=(const ThreadPool&) = delete;

    ThreadPool(ThreadPool&&) = delete;

    ThreadPool& operator=(ThreadPool&&) = delete;

    ThreadPool(AsyncHandler* asyncHandler);

    ~ThreadPool() = default;

    void joinAll();

private:
    AsyncHandler* asyncHandler;
    std::vector<std::thread> threads;
};

}

#endif
