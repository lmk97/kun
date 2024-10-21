#ifndef KUN_LOOP_THREAD_POOL_H
#define KUN_LOOP_THREAD_POOL_H

#include <condition_variable>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

#include "loop/async_request.h"

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

    std::list<AsyncRequest> getResolvedRequests();

    void submit(AsyncRequest&& req);

    bool tryClose();

    AsyncHandler* const asyncHandler;
    std::vector<std::thread> threads;
    std::list<AsyncRequest> pendingRequests;
    std::list<AsyncRequest> resolvedRequests;
    std::condition_variable pendingCond;
    std::mutex pendingMutex;
    std::mutex resolvedMutex;
    int busyCount{0};
    bool notified{false};
    bool closed{false};
};

}

#endif
