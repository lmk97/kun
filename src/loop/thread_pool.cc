#include "loop/thread_pool.h"

#include <stddef.h>

#include <mutex>

#include "env/environment.h"
#include "loop/async_handler.h"

using kun::AsyncHandler;
using kun::Environment;

namespace {

void handleAsyncRequest(AsyncHandler* asyncHandler) {
    while (true) {
        std::unique_lock<std::mutex> lock(asyncHandler->handleMutex);
        auto& handleCond = asyncHandler->handleCond;
        auto& handleRequests = asyncHandler->handleRequests;
        asyncHandler->busyCount++;
        while (handleRequests.empty()) {
            if (asyncHandler->closed) {
                break;
            }
            asyncHandler->busyCount--;
            handleCond.wait(lock);
            asyncHandler->busyCount++;
        }
        if (asyncHandler->closed) {
            break;
        }
        auto asyncRequest = std::move(handleRequests.front());
        handleRequests.pop_front();
        if (!handleRequests.empty()) {
            handleCond.notify_one();
        }
        lock.unlock();
        asyncRequest.handle();
        std::lock_guard<std::mutex> lockGuard(asyncHandler->resolveMutex);
        asyncHandler->resolveRequests.emplace_back(std::move(asyncRequest));
        if (!asyncHandler->notified) {
            asyncHandler->notify();
            asyncHandler->notified = true;
        }
    }
}

}

namespace kun {

ThreadPool::ThreadPool(AsyncHandler* asyncHandler) : asyncHandler(asyncHandler) {
    auto cmdline = asyncHandler->env->getCmdline();
    auto size = cmdline->get<int>(Cmdline::THREAD_POOL_SIZE).unwrap();
    if (size < 2) {
        size = 4;
    }
    threads.reserve(static_cast<size_t>(size));
    for (int i = 0; i < size; i++) {
        threads.emplace_back(handleAsyncRequest, asyncHandler);
    }
}

void ThreadPool::joinAll() {
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    threads.clear();
}

}
