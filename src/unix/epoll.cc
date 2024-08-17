#include "unix/epoll.h"

#ifdef KUN_PLATFORM_LINUX

#include <errno.h>
#include <unistd.h>
#include <sys/timerfd.h>
#include <sys/signal.h>

#include "sys/io.h"
#include "loop/timer.h"
#include "util/sys_err.h"
#include "util/scope_guard.h"

using kun::sys::eprintln;

namespace kun {

EventLoop::EventLoop(Environment* env) : env(env) {
    backendFd = ::epoll_create1(EPOLL_CLOEXEC);
    if (backendFd != -1) {

    } else {
        auto [code, phrase] = SysErr(errno);
        eprintln("ERROR: 'epoll_create1' ({}) {}", code, phrase);
    }
}

EventLoop::~EventLoop() {
    if (backendFd != -1 && ::close(backendFd) == -1) {
        auto [code, phrase] = SysErr(errno);
        eprintln("ERROR: 'close' ({}) {}", code, phrase);
    }
}

void EventLoop::run() {
    if (backendFd == -1 || channelCount == 0) {
        return;
    }
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    ::sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (::sigaction(SIGPIPE, &sa, nullptr) == -1) {
        auto [code, phrase] = SysErr(errno);
        eprintln("ERROR: 'sigaction' ({}) {}", code, phrase);
        return;
    }
    auto options = env->getOptions();
    auto maxEvents = options->get<int>(OptionName::MAX_CLIENT_SIZE, 1024);
    if (maxEvents <= 0) {
        maxEvents = 1024;
    }
    auto epollEvents = new struct epoll_event[maxEvents];
    ON_SCOPE_EXIT {
        delete[] epollEvents;
    };
    int nfds = 0;
    while (true) {
        nfds = ::epoll_wait(backendFd, epollEvents, maxEvents, -1);
        if (nfds == -1) {
            if (errno == EINTR) {
                continue;
            }
            auto [code, phrase] = SysErr(errno);
            eprintln("ERROR: 'epoll_wait' ({}) {}", code, phrase);
            break;
        }
        for (int i = 0; i < nfds; i++) {
            auto events = epollEvents[i].events;
            auto channel = static_cast<Channel*>(epollEvents[i].data.ptr);
            if (channel->type == ChannelType::TIMER) {
                auto timer = static_cast<Timer*>(channel);
                uint64_t value;
                ::read(timer->fd, &value, sizeof(value));
                if (!timer->interval) {
                    removeChannel(timer);
                }
            }
            if (events & EPOLLIN) {
                channel->onReadable();
            } else if (events & EPOLLOUT) {
                channel->onWritable();
            } else if (events & (EPOLLERR | EPOLLHUP)) {
                channel->onError();
            }
        }
        if (channelCount == 0) {
            break;
        }
    }
}

bool EventLoop::addChannel(Channel* channel) {
    if (channel->fd == KUN_INVALID_FD) {
        eprintln("ERROR: 'addChannel' invalid fd");
        return false;
    }
    struct epoll_event ev;
    if (channel->type == ChannelType::READ) {
        ev.events = EPOLLET | EPOLLIN;
    } else if (channel->type == ChannelType::WRITE) {
        ev.events = EPOLLET | EPOLLOUT;
    } else if (channel->type == ChannelType::TIMER) {
        ev.events = EPOLLET | EPOLLIN;
        auto timer = static_cast<Timer*>(channel);
        auto interval = timer->interval;
        auto s = timer->milliseconds / 1000;
        auto ns = timer->nanoseconds;
        struct itimerspec newValue;
        newValue.it_interval.tv_sec = interval ? s : 0;
        newValue.it_interval.tv_nsec = interval ? ns : 0;
        newValue.it_value.tv_sec = s;
        newValue.it_value.tv_nsec = ns;
        if (::timerfd_settime(timer->fd, 0, &newValue, nullptr) == -1) {
            auto [code, phrase] = SysErr(errno);
            eprintln("ERROR: 'timerfd_settime' ({}) {}", code, phrase);
            return false;
        }
    } else {
        eprintln("ERROR: 'addChannel' invalid channel type");
        return false;
    }
    ev.data.ptr = channel;
    if (::epoll_ctl(backendFd, EPOLL_CTL_ADD, channel->fd, &ev) == -1) {
        auto [code, phrase] = SysErr(errno);
        eprintln("ERROR: 'epoll_ctl' ({}) {}", code, phrase);
        return false;
    }
    channelCount++;
    return true;
}

bool EventLoop::modifyChannel(Channel* channel) {
    if (channel->fd == KUN_INVALID_FD) {
        eprintln("ERROR: 'modifyChannel' invalid fd");
        return false;
    }
    struct epoll_event ev;
    if (channel->type == ChannelType::READ) {
        ev.events = EPOLLET | EPOLLIN;
    } else if (channel->type == ChannelType::WRITE) {
        ev.events = EPOLLET | EPOLLOUT;
    } else {
        eprintln("ERROR: 'modifyChannel' invalid channel type");
        return false;
    }
    ev.data.ptr = channel;
    if (::epoll_ctl(backendFd, EPOLL_CTL_MOD, channel->fd, &ev) == -1) {
        auto [code, phrase] = SysErr(errno);
        eprintln("ERROR: 'epoll_ctl' ({}) {}", code, phrase);
        return false;
    }
    return true;
}

bool EventLoop::removeChannel(Channel* channel) {
    if (channel->fd == KUN_INVALID_FD) {
        eprintln("ERROR: 'removeChannel' invalid fd");
        return false;
    }
    struct epoll_event ev;
    ev.events = EPOLLET | EPOLLIN;
    ev.data.ptr = channel;
    if (::epoll_ctl(backendFd, EPOLL_CTL_DEL, channel->fd, &ev) == -1) {
        auto [code, phrase] = SysErr(errno);
        eprintln("ERROR: 'epoll_ctl' ({}) {}", code, phrase);
        return false;
    }
    if (channelCount > 0) {
        channelCount--;
    }
    return true;
}

}

#endif
