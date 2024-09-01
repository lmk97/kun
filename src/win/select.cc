#include "win/select.h"

#ifdef KUN_PLATFORM_WIN32

#include <stdlib.h>

#include "util/scope_guard.h"
#include "util/sys_err.h"
#include "util/util.h"
#include "win/err.h"

using kun::SysErr;
using kun::sys::microsecond;
using kun::win::convertError;

namespace {

class EventData {
public:
    EventData(const EventData&) = delete;

    EventData& operator=(const EventData&) = delete;

    EventData(EventData&&) = delete;

    EventData& operator=(EventData&&) = delete;

    EventData(u_int nfds) : size(nfds) {
        auto p = malloc(sizeof(u_int) + sizeof(SOCKET) * size);
        if (p != nullptr) {
            fds = new (p) struct fd_set();
        } else {
            KUN_LOG_ERR("'malloc' out of memory");
        }
    }

    ~EventData() {
        if (fds != nullptr) {
            delete[] fds;
        }
    }

    void realloc(u_int nfds) {
        if (nfds <= size) {
            return;
        }
        auto p = ::realloc(fds, sizeof(u_int) + sizeof(SOCKET) * nfds);
        if (p == nullptr) {
            KUN_LOG_ERR("'realloc' out of memory");
            return;
        }
        fds = new (p) struct fd_set();
        size = nfds;
    }

    struct fd_set* fds{nullptr};
    u_int size;
};

}

namespace kun {

EventLoop::EventLoop(Environment* env) :
    env(env),
    asyncHandler(env),
    usecTimers(1024)
{
    fdChannelMap.reserve(1024);
}

void EventLoop::run() {
    if (fdChannelMap.empty() && usecTimers.empty()) {
        return;
    }
    EventData readData(1024);
    EventData writeData(1024);
    if (!addChannel(&asyncHandler)) {
        KUN_LOG_ERR("EventLoop.run");
        return;
    }
    struct timeval tv;
    int nfds = 0;
    while (true) {
        auto readFds = readData.fds;
        auto writeFds = writeData.fds;
        readFds->fd_count = 0;
        writeFds->fd_count = 0;
        for (const auto& [fd, channel] : fdChannelMap) {
            if (channel->type == ChannelType::READ) {
                if (readFds->fd_count >= readData.size) {
                    readData.realloc(readData.size + 1024);
                    readFds = readData.fds;
                }
                readFds->fd_array[readFds->fd_count++] = fd;
            } else if (channel->type == ChannelType::WRITE) {
                if (writeFds->fd_count >= writeData.size) {
                    writeData.realloc(writeData.size + 1024);
                    writeFds = writeData.fds;
                }
                writeFds->fd_array[writeFds->fd_count++] = fd;
            }
        }
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
        nfds = ::select(
            0,
            readFds->fd_count > 0 ? readFds : nullptr,
            writeFds->fd_count > 0 ? writeFds : nullptr,
            nullptr,
            timeout
        );
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
        for (u_int i = 0; i < readFds->fd_count; i++) {
            auto iter = fdChannelMap.find(readFds->fd_array[i]);
            if (iter != fdChannelMap.end()) {
                iter->second->onReadable();
            }
        }
        for (u_int i = 0; i < writeFds->fd_count; i++) {
            auto iter = fdChannelMap.find(writeFds->fd_array[i]);
            if (iter != fdChannelMap.end()) {
                iter->second->onWritable();
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
        if (auto result = sys::nanosecond()) {
            auto ts = result.unwrap();
            auto ms = timer->milliseconds;
            auto ns = timer->nanoseconds;
            ms += static_cast<uint64_t>(ts.tv_sec * 1000);
            ns += static_cast<uint64_t>(ts.tv_nsec);
            usecTimers.push(timer, ms * 1000 + ns / 1000);
            return true;
        } else {
            return false;
        }
    } else {
        if (channel->fd == KUN_INVALID_FD) {
            KUN_LOG_ERR("'addChannel' invalid fd");
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
