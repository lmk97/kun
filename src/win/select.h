#ifndef KUN_WIN_SELECT_H
#define KUN_WIN_SELECT_H

#include "util/constants.h"

#ifdef KUN_PLATFORM_WIN32

#include <stddef.h>
#include <stdint.h>
#include <winsock2.h>

#include <unordered_map>

#include "env/environment.h"
#include "loop/async_handler.h"
#include "loop/channel.h"
#include "loop/timer.h"
#include "sys/io.h"
#include "sys/time.h"
#include "util/min_heap.h"

namespace kun {

class EventLoop {
public:
    EventLoop(const EventLoop&) = delete;

    EventLoop& operator=(const EventLoop&) = delete;

    EventLoop(EventLoop&&) = delete;

    EventLoop& operator=(EventLoop&&) = delete;

    EventLoop(Environment* env);

    ~EventLoop() = default;

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
    MinHeap<Timer, uint64_t> usecTimers;
    std::unordered_map<SOCKET, Channel*> fdChannelMap;
};

}

#endif

#endif
