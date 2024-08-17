#ifndef KUN_LOOP_ASYNC_EVENT_H
#define KUN_LOOP_ASYNC_EVENT_H

#include <stddef.h>

#include <deque>
#include <mutex>
#include <condition_variable>

#include "env/environment.h"
#include "loop/async_request.h"
#include "loop/channel.h"
#include "loop/thread_pool.h"

namespace kun {

class AsyncEvent : public Channel {
public:
    AsyncEvent(Environment* env);

    void onReadable() override final;

    void notify();

    void submit(AsyncRequest&& asyncRequest);

    bool tryClose();

    Environment* env;
    ThreadPool threadPool;
    std::deque<AsyncRequest> asyncRequests;
    std::mutex handleMutex;
    std::condition_variable handleCond;
    std::mutex notifyMutex;
    size_t handleIndex{0};
    size_t handledCount{0};
    bool notified{false};
    bool closed{false};
};

}

#endif
