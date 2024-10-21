#include "loop/thread_pool.h"

#include <stddef.h>

#include "env/cmdline.h"
#include "env/environment.h"
#include "loop/async_handler.h"

using kun::ThreadPool;

namespace {

void handleAsyncRequest(ThreadPool* threadPool) {
    bool isFirst = true;
    while (true) {
        std::unique_lock<std::mutex> lock(threadPool->pendingMutex);
        auto& pendingCond = threadPool->pendingCond;
        auto& pendingRequests = threadPool->pendingRequests;
        if (isFirst) {
            threadPool->busyCount++;
        }
        while (pendingRequests.empty()) {
            if (threadPool->closed) {
                break;
            }
            threadPool->busyCount--;
            pendingCond.wait(lock);
            threadPool->busyCount++;
        }
        if (threadPool->closed) {
            break;
        }
        auto asyncRequest = std::move(pendingRequests.front());
        pendingRequests.pop_front();
        if (!pendingRequests.empty()) {
            pendingCond.notify_one();
        }
        lock.unlock();
        asyncRequest.handle();
        {
            std::lock_guard<std::mutex> lockGuard(threadPool->resolvedMutex);
            threadPool->resolvedRequests.emplace_back(std::move(asyncRequest));
            if (!threadPool->notified) {
                threadPool->asyncHandler->notify();
                threadPool->notified = true;
            }
        }
        if (isFirst) {
            isFirst = false;
        }
    }
}

}

namespace kun {

ThreadPool::ThreadPool(AsyncHandler* asyncHandler) : asyncHandler(asyncHandler) {
    auto env = asyncHandler->getEnvironment();
    auto cmdline = env->getCmdline();
    auto size = cmdline->get<size_t>(Cmdline::THREAD_POOL_SIZE).unwrap();
    threads.reserve(size);
    for (size_t i = 0; i < size; i++) {
        threads.emplace_back(handleAsyncRequest, this);
    }
}

std::list<AsyncRequest> ThreadPool::getResolvedRequests() {
    std::list<AsyncRequest> requests;
    {
        std::lock_guard<std::mutex> lockGuard(resolvedMutex);
        requests.swap(resolvedRequests);
        notified = false;
    }
    return requests;
}

void ThreadPool::submit(AsyncRequest&& req) {
    std::lock_guard<std::mutex> lockGuard(pendingMutex);
    pendingRequests.emplace_back(std::move(req));
    pendingCond.notify_one();
}

bool ThreadPool::tryClose() {
    bool idle = false;
    {
        std::lock_guard<std::mutex> lockGuard(pendingMutex);
        if (closed) {
            return true;
        }
        idle = pendingRequests.empty() && busyCount == 0;
    }
    bool hasReq = false;
    if (idle) {
        std::lock_guard<std::mutex> lockGuard(resolvedMutex);
        hasReq = !resolvedRequests.empty();
    }
    if (idle && !hasReq) {
        std::lock_guard<std::mutex> lockGuard(pendingMutex);
        closed = true;
        pendingCond.notify_all();
    }
    if (idle && !hasReq) {
        for (auto& t : threads) {
            if (t.joinable()) {
                t.join();
            }
        }
        threads.clear();
        return true;
    }
    return false;
}

}
