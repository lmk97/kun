#ifndef KUN_LOOP_ASYNC_HANDLER_H
#define KUN_LOOP_ASYNC_HANDLER_H

#include <stddef.h>

#include <condition_variable>
#include <list>
#include <mutex>

#include "env/environment.h"
#include "loop/async_request.h"
#include "loop/channel.h"
#include "loop/thread_pool.h"

namespace kun {

class AsyncHandler : public Channel {
public:
    AsyncHandler(Environment* env);

    ~AsyncHandler();

    void onReadable() override final;

    void notify();

    void submit(AsyncRequest&& req);

    bool tryClose();

    Environment* env;
    std::list<AsyncRequest> handleRequests;
    std::list<AsyncRequest> resolveRequests;
    std::condition_variable handleCond;
    std::mutex handleMutex;
    std::mutex resolveMutex;
    int busyCount{0};
    bool notified{false};
    bool closed{false};
    ThreadPool threadPool;
};

}

#endif
