#ifndef KUN_UNIX_EPOLL_H
#define KUN_UNIX_EPOLL_H

#include "util/constants.h"

#ifdef KUN_PLATFORM_LINUX

#include <stdint.h>
#include <sys/epoll.h>

#include "env/environment.h"
#include "loop/async_handler.h"
#include "loop/channel.h"

namespace kun {

class EventLoop {
public:
    EventLoop(const EventLoop&) = delete;

    EventLoop& operator=(const EventLoop&) = delete;

    EventLoop(EventLoop&&) = delete;

    EventLoop& operator=(EventLoop&&) = delete;

    EventLoop(Environment* env);

    ~EventLoop();

    void run();

    bool addChannel(Channel* channel);

    bool modifyChannel(Channel* channel);

    bool removeChannel(Channel* channel);

    void submitAsyncRequest(AsyncRequest&& req) {
        asyncHandler.submit(std::move(req));
    }

private:
    Environment* env;
    AsyncHandler asyncHandler;
    uint32_t channelCount{0};
    int backendFd;
};

}

#endif

#endif
