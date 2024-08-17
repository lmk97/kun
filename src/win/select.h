#ifndef KUN_WIN_SELECT_H
#define KUN_WIN_SELECT_H

#include "util/constants.h"

#ifdef KUN_PLATFORM_WIN32

#include <stdint.h>
#include <stddef.h>
#include <winsock2.h>

#include <unordered_map>

#include "env/environment.h"
#include "loop/channel.h"
#include "loop/timer.h"
#include "util/min_heap.h"
#include "sys/io.h"
#include "sys/time.h"

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

private:
    Environment* env;
    MinHeap<Timer, uint64_t> usecTimers;
    std::unordered_map<SOCKET, Channel*> fdChannelMap;
};

}

#endif

#endif
