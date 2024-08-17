#ifndef KUN_LOOP_THREAD_POOL_H
#define KUN_LOOP_THREAD_POOL_H

#include <stdint.h>

#include <vector>
#include <thread>

#include "env/environment.h"

namespace kun {

class AsyncEvent;

class ThreadPool {
public:
    ThreadPool(const ThreadPool&) = delete;

    ThreadPool& operator=(const ThreadPool&) = delete;

    ThreadPool(ThreadPool&&) = delete;

    ThreadPool& operator=(ThreadPool&&) = delete;

    ThreadPool(AsyncEvent* asyncEvent) : asyncEvent(asyncEvent) {}

    void init(uint32_t size);

    void joinAll();

private:
    AsyncEvent* asyncEvent;
    std::vector<std::thread> threads;
};

}

#endif
