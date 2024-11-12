#include "win/select.h"

#ifdef KUN_PLATFORM_WIN32

#include <stdlib.h>

#include "util/scope_guard.h"
#include "util/sys_err.h"
#include "util/utils.h"
#include "win/err.h"

using kun::SysErr;
using kun::sys::microsecond;
using kun::win::convertError;

namespace {

class FdsWrap {
public:
    FdsWrap(const FdsWrap&) = delete;

    FdsWrap& operator=(const FdsWrap&) = delete;

    FdsWrap(FdsWrap&&) = delete;

    FdsWrap& operator=(FdsWrap&&) = delete;

    FdsWrap(u_int nfds) {
        auto p = malloc(sizeof(u_int) + sizeof(SOCKET) * nfds);
        if (p != nullptr) {
            fds = static_cast<struct fd_set*>(p);
            maxFds = nfds;
        } else {
            KUN_LOG_ERR("out of memory");
        }
    }

    ~FdsWrap() {
        free(fds);
    }

    struct fd_set* data() const {
        return fds;
    }

    void realloc(u_int nfds) {
        if (nfds <= maxFds) {
            return;
        }
        auto p = ::realloc(fds, sizeof(u_int) + sizeof(SOCKET) * nfds);
        if (p == nullptr) {
            KUN_LOG_ERR("out of memory");
            return;
        }
        fds = static_cast<struct fd_set*>(p);
        maxFds = nfds;
    }

    void push(SOCKET fd) {
        if (fds->fd_count >= maxFds) {
            this->realloc(maxFds + 1024);
        }
        fds->fd_array[fds->fd_count++] = fd;
    }

    void clear() {
        fds->fd_count = 0;
    }

private:
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
        KUN_LOG_ERR("Failed to add AsyncHandler");
    }
}

void EventLoop::run() {
    if (fdChannelMap.size() <= 1 && usecTimers.empty()) {
        return;
    }
    FdsWrap readFdsWrap(1024);
    FdsWrap writeFdsWrap(1024);
    struct timeval tv;
    int nfds = 0;
    while (true) {
        readFdsWrap.clear();
        writeFdsWrap.clear();
        for (const auto& [fd, channel] : fdChannelMap) {
            if (channel->type == ChannelType::READ) {
                readFdsWrap.push(fd);
            } else if (channel->type == ChannelType::WRITE) {
                writeFdsWrap.push(fd);
            }
        }
        auto readfds = readFdsWrap.data();
        auto writefds = writeFdsWrap.data();
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
        nfds = ::select(0, readfds, writefds, nullptr, timeout);
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
        if (readfds != nullptr) {
            auto fdCount = readfds->fd_count;
            auto fdArray = readfds->fd_array;
            for (u_int i = 0; i < fdCount; i++) {
                auto iter = fdChannelMap.find(fdArray[i]);
                if (iter != fdChannelMap.end()) {
                    iter->second->onReadable();
                }
            }
        }
        if (writefds != nullptr) {
            auto fdCount = writefds->fd_count;
            auto fdArray = writefds->fd_array;
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
                    if (timer->repeat) {
                        auto value = timer->getValue(TimeUnit::MICROSECOND);
                        usecTimers.push(timer, currTime + value);
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
        auto currTime = microsecond().unwrap();
        auto value = timer->getValue(TimeUnit::MICROSECOND);
        usecTimers.push(timer, currTime + value);
        return true;
    }
    if (channel->fd == KUN_INVALID_FD) {
        KUN_LOG_ERR("invalid fd");
        return false;
    }
    auto pair = fdChannelMap.emplace(channel->fd, channel);
    return pair.second;
}

bool EventLoop::modifyChannel(Channel* channel) {
    return true;
}

bool EventLoop::removeChannel(Channel* channel) {
    if (channel->type == ChannelType::TIMER) {
        auto timer = static_cast<Timer*>(channel);
        timer->removed = true;
        return true;
    }
    if (channel->fd == KUN_INVALID_FD) {
        KUN_LOG_ERR("invalid fd");
        return false;
    }
    return fdChannelMap.erase(channel->fd) == 1;
}

}

#endif
