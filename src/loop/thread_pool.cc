#include "loop/thread_pool.h"

#include <stddef.h>

#include <mutex>

#include "loop/async_event.h"

using kun::Environment;
using kun::AsyncEvent;

namespace {

void handleAsyncRequest(AsyncEvent* asyncEvent) {
    while (true) {
        std::unique_lock<std::mutex> lock(asyncEvent->handleMutex);
        auto& handleCond = asyncEvent->handleCond;
        auto& asyncRequests = asyncEvent->asyncRequests;
        while (asyncEvent->handleIndex >= asyncRequests.size()) {
            if (asyncEvent->closed) {
                break;
            }
            handleCond.wait(lock);
        }
        if (asyncEvent->closed) {
            break;
        }
        auto& asyncRequest = asyncRequests[asyncEvent->handleIndex++];
        if (asyncEvent->handleIndex < asyncRequests.size()) {
            handleCond.notify_one();
        }
        lock.unlock();
        asyncRequest.handle();
        std::lock_guard<std::mutex> lockGuard(asyncEvent->notifyMutex);
        asyncEvent->handledCount++;
        if (!asyncEvent->notified) {
            asyncEvent->notify();
            asyncEvent->notified = true;
        }
    }
}

}

namespace kun {

void ThreadPool::init(uint32_t size) {
    threads.reserve(static_cast<size_t>(size));
    for (uint32_t i = 0; i < size; i++) {
        threads.emplace_back(handleAsyncRequest, asyncEvent);
    }
}

void ThreadPool::joinAll() {
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
}

}
