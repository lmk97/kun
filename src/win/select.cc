#include "win/select.h"

#ifdef KUN_PLATFORM_WIN32

#include <stdlib.h>

#include "util/scope_guard.h"
#include "util/sys_err.h"
#include "util/utils.h"
#include "win/err.h"

using kun::SysErr;
using kun::sys::microsecond;
using kun::sys::nanosecond;
using kun::win::convertError;

namespace {

class EventData {
public:
    EventData(const EventData&) = delete;

    EventData& operator=(const EventData&) = delete;

    EventData(EventData&&) = delete;

    EventData& operator=(EventData&&) = delete;

    EventData(u_int nfds) {
        auto p = malloc(sizeof(u_int) + sizeof(SOCKET) * nfds);
        if (p != nullptr) {
            buf = p;
            fds = new (p) struct fd_set();
            maxFds = nfds;
        } else {
            KUN_LOG_ERR("'EventData::EventData' out of memory");
        }
    }

    ~EventData() {
        if (fds != nullptr) {
            fds->~fd_set();
        }
        if (buf != nullptr) {
            free(buf);
        }
    }

    struct fd_set* getFds() const {
        return fds->fd_count > 0 ? fds : nullptr;
    }

    void realloc(u_int nfds) {
        if (nfds <= maxFds) {
            return;
        }
        auto p = malloc(sizeof(u_int) + sizeof(SOCKET) * nfds);
        if (p == nullptr) {
            KUN_LOG_ERR("'EventData::realloc' out of memory");
            return;
        }
        auto newFds = new (p) struct fd_set();
        if (fds != nullptr) {
            newFds->fd_count = fds->fd_count;
            for (u_int i = 0; i < fds->fd_count; i++) {
                newFds->fd_array[i] = fds->fd_array[i];
            }
            fds->~fd_set();
        }
        if (buf != nullptr) {
            free(buf);
        }
        buf = p;
        fds = newFds;
        maxFds = nfds;
    }

    void push(SOCKET fd) {
        if (fds->fd_count >= maxFds) {
            this->realloc(maxFds + 512);
        }
        fds->fd_array[fds->fd_count++] = fd;
    }

    void clear() {
        fds->fd_count = 0;
    }

private:
    void* buf{nullptr};
    struct fd_set* fds{nullptr};
    u_int maxFds{0};
};

}

namespace kun {

EventLoop::EventLoop(Environment* env) :
    env(env),
    asyncHandler(env),
    usecTimers(1024)
{
    fdChannelMap.reserve(1024);
    if (!addChannel(&asyncHandler)) {
        KUN_LOG_ERR("EventLoop::EventLoop");
    }
}

void EventLoop::run() {
    if (fdChannelMap.size() <= 1 && usecTimers.empty()) {
        return;
    }
    EventData readEventData(1024);
    EventData writeEventData(1024);
    struct timeval tv;
    int nfds = 0;
    while (true) {
        readEventData.clear();
        writeEventData.clear();
        for (const auto& [fd, channel] : fdChannelMap) {
            if (channel->type == ChannelType::READ) {
                readEventData.push(fd);
            } else if (channel->type == ChannelType::WRITE) {
                writeEventData.push(fd);
            }
        }
        auto readFds = readEventData.getFds();
        auto writeFds = writeEventData.getFds();
        struct timeval* timeout = nullptr;
        if (!usecTimers.empty()) {
            auto currTime = microsecond().unwrap();
            auto data = usecTimers.peek();
            uint64_t us = 0;
            if (data.num >= currTime) {
                us = data.num - currTime;
            }
            tv.tv_sec = static_cast<long>(us / 1000000);
            tv.tv_usec = static_cast<long>(us - tv.tv_sec * 1000000);
            timeout = &tv;
        }
        nfds = ::select(0, readFds, writeFds, nullptr, timeout);
        if (nfds == SOCKET_ERROR) {
            auto errCode = ::WSAGetLastError();
            if (errCode == WSAEINTR) {
                continue;
            }
            if (errCode != WSAEINVAL) {
                KUN_LOG_ERR(convertError(errCode));
                break;
            }
        }
        if (readFds != nullptr) {
            auto fdCount = readFds->fd_count;
            auto fdArray = readFds->fd_array;
            for (u_int i = 0; i < fdCount; i++) {
                auto iter = fdChannelMap.find(fdArray[i]);
                if (iter != fdChannelMap.end()) {
                    iter->second->onReadable();
                }
            }
        }
        if (writeFds != nullptr) {
            auto fdCount = writeFds->fd_count;
            auto fdArray = writeFds->fd_array;
            for (u_int i = 0; i < fdCount; i++) {
                auto iter = fdChannelMap.find(fdArray[i]);
                if (iter != fdChannelMap.end()) {
                    iter->second->onWritable();
                }
            }
        }
        if (timeout != nullptr) {
            auto currTime = microsecond().unwrap();
            while (!usecTimers.empty()) {
                auto [timer, usec] = usecTimers.peek();
                if (usec > currTime) {
                    break;
                }
                usecTimers.pop();
                if (!timer->removed) {
                    timer->onReadable();
                    if (timer->interval) {
                        auto ms = timer->milliseconds;
                        auto ns = timer->nanoseconds;
                        auto us = currTime + (ms * 1000 + ns / 1000);
                        usecTimers.push(timer, us);
                    }
                }
            }
        }
        if (fdChannelMap.size() <= 1 && usecTimers.empty()) {
            if (asyncHandler.tryClose()) {
                break;
            }
        }
    }
}

bool EventLoop::addChannel(Channel* channel) {
    if (channel->type == ChannelType::TIMER) {
        auto timer = static_cast<Timer*>(channel);
        auto ms = timer->milliseconds;
        auto ns = timer->nanoseconds;
        auto ts = nanosecond().unwrap();
        ms += static_cast<uint64_t>(ts.tv_sec * 1000);
        ns += static_cast<uint64_t>(ts.tv_nsec);
        usecTimers.push(timer, ms * 1000 + ns / 1000);
        return true;
    } else {
        if (channel->fd == KUN_INVALID_FD) {
            KUN_LOG_ERR("'EventLoop::addChannel' invalid fd");
            return false;
        }
        auto pair = fdChannelMap.emplace(channel->fd, channel);
        return pair.second;
    }
}

bool EventLoop::modifyChannel(Channel* channel) {
    return true;
}

bool EventLoop::removeChannel(Channel* channel) {
    if (channel->type == ChannelType::TIMER) {
        auto timer = static_cast<Timer*>(channel);
        timer->removed = true;
        return true;
    } else {
        return fdChannelMap.erase(channel->fd) == 1;
    }
}

}

#endif
