#ifndef KUN_LOOP_ASYNC_HANDLER_H
#define KUN_LOOP_ASYNC_HANDLER_H

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

    Environment* getEnvironment() const {
        return env;
    }

    void submit(AsyncRequest&& req) {
        threadPool.submit(std::move(req));
    }

    bool tryClose() {
        return threadPool.tryClose();
    }

private:
    Environment* env;
    ThreadPool threadPool;
};

}

#endif
