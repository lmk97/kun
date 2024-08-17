#include "win/select.h"

#ifdef KUN_PLATFORM_WIN32

#include <stdlib.h>

#include "util/sys_err.h"
#include "util/scope_guard.h"
#include "win/err.h"

using kun::SysErr;
using kun::sys::eprintln;
using kun::sys::microsecond;
using kun::win::convertError;

namespace {

inline fd_set* allocFdSet(size_t nfds) {
    auto p = malloc(sizeof(u_int) + sizeof(SOCKET) * nfds);
    if (p == nullptr) {
        eprintln("ERROR: 'allocFdSet' out of memory");
    }
    return static_cast<fd_set*>(p);
}

inline fd_set* reallocFdSet(fd_set* fds, size_t nfds) {
    auto p = realloc(fds, sizeof(u_int) + sizeof(SOCKET) * nfds);
    if (p == nullptr) {
        eprintln("ERROR: 'reallocFdSet' out of memory");
        return fds;
    }
    return static_cast<fd_set*>(p);
}

}

namespace kun {

EventLoop::EventLoop(Environment* env) : env(env), usecTimers(1024) {
    fdChannelMap.reserve(1024);
}

EventLoop::~EventLoop() {

}

void EventLoop::run() {
    auto readFds = allocFdSet(1024);
    auto writeFds = allocFdSet(1024);
    ON_SCOPE_EXIT {
        free(readFds);
        free(writeFds);
    };
    struct timeval tv;
    int nfds = 0;
    while (true) {
        if (fdChannelMap.empty() && usecTimers.empty()) {
            break;
        }
        readFds->fd_count = 0;
        writeFds->fd_count = 0;
        for (const auto& [fd, channel] : fdChannelMap) {
            if (channel->type == ChannelType::READ) {
                readFds->fd_array[readFds->fd_count++] = fd;
            } else if (channel->type == ChannelType::WRITE) {
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
                auto [code, phrase] = SysErr(convertError(errCode));
                eprintln("ERROR: 'select' ({}) {}", code, phrase);
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
                    if (timer->interval) {
                        auto ms = timer->milliseconds;
                        auto ns = timer->nanoseconds;
                        auto us = currTime + (ms * 1000 + ns / 1000);
                        usecTimers.push(timer, us);
                    }
                    timer->onReadable();
                }
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
            eprintln("ERROR: 'addChannel' invalid fd");
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
