#ifndef KUN_LOOP_TIMER_H
#define KUN_LOOP_TIMER_H

#include <stdint.h>
#include <errno.h>

#if defined(KUN_PLATFORM_LINUX)
#include <sys/timerfd.h>
#endif

#include "util/constants.h"
#include "util/sys_err.h"
#include "loop/channel.h"
#include "sys/io.h"
#include "sys/time.h"

namespace kun {

class Timer : public Channel {
public:
    Timer(uint64_t milliseconds, uint64_t nanoseconds, bool interval);

    Timer(uint64_t milliseconds, bool interval) : Timer(milliseconds, 0, interval) {}

    virtual ~Timer() = 0;

    uint64_t milliseconds;
    uint64_t nanoseconds;
    bool interval;
#ifdef KUN_PLATFORM_WIN32
    bool removed{false};
#endif
};

inline Timer::Timer(
    uint64_t milliseconds,
    uint64_t nanoseconds,
    bool interval
) :
    Channel(KUN_INVALID_FD, ChannelType::TIMER),
    milliseconds(milliseconds),
    nanoseconds(nanoseconds),
    interval(interval)
{
#if defined(KUN_PLATFORM_LINUX)
    fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
    if (fd == -1) {
        auto [code, phrase] = SysErr(errno);
        sys::eprintln("ERROR: 'timerfd_create' ({}) {}", code, phrase);
    }
#endif
}

inline Timer::~Timer() {

}

}

#endif
