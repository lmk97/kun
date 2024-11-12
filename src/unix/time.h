#ifndef KUN_UNIX_TIME_H
#define KUN_UNIX_TIME_H

#include "util/constants.h"

#ifdef KUN_PLATFORM_UNIX

#include <errno.h>
#include <stdint.h>
#include <time.h>

#include "util/result.h"

namespace KUN_SYS {

inline Result<uint64_t> microsecond() {
    struct timespec ts;
    #ifdef KUN_PLATFORM_LINUX
    if (::clock_gettime(CLOCK_MONOTONIC_COARSE, &ts) == -1) {
        return SysErr(errno);
    }
    #else
    if (::clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return SysErr(errno);
    }
    #endif
    uint64_t us = ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
    return us;
}

inline Result<struct timespec> nanosecond() {
    struct timespec ts;
    if (::clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return ts;
    }
    return SysErr(errno);
}

}

#endif

#endif
