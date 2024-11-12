#ifndef KUN_LOOP_TIMER_H
#define KUN_LOOP_TIMER_H

#include <errno.h>
#include <stdint.h>

#if defined(KUN_PLATFORM_LINUX)
#include <sys/timerfd.h>
#endif

#include "loop/channel.h"
#include "util/constants.h"
#include "util/utils.h"

namespace kun {

enum class TimeUnit {
    NANOSECOND = 1,
    MICROSECOND = 1000,
    MILLISECOND = 1000000
};

class Timer : public Channel {
public:
    Timer(uint64_t value, TimeUnit timeUnit, bool repeat) :
        Channel(KUN_INVALID_FD, ChannelType::TIMER),
        value(value),
        timeUnit(timeUnit),
        repeat(repeat)
    {
        #ifdef KUN_PLATFORM_LINUX
        fd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
        if (fd == -1) {
            KUN_LOG_ERR(errno);
        }
        #endif
    }

    virtual ~Timer() = 0;

    uint64_t getValue(TimeUnit timeUnit) const {
        return value * static_cast<int>(this->timeUnit) / static_cast<int>(timeUnit);
    }

    const uint64_t value;
    const TimeUnit timeUnit;
    const bool repeat;
    #ifdef KUN_PLATFORM_WIN32
    bool removed{false};
    #endif
};

inline Timer::~Timer() {

}

}

#endif
