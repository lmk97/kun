#ifndef KUN_UNIX_TIME_H
#define KUN_UNIX_TIME_H

#include "util/constants.h"

#ifdef KUN_PLATFORM_UNIX

#include <errno.h>
#include <stdint.h>
#include <time.h>

#include "util/result.h"

namespace KUN_SYS {

inline Result<struct timespec> nanosecond() {
    struct timespec ts;
    #ifdef KUN_PLATFORM_LINUX
    if (::clock_gettime(CLOCK_MONOTONIC_COARSE, &ts) == 0) {
        return ts;
    }
    #endif
    if (::clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
        return ts;
    }
    return SysErr(errno);
}

}

#endif

#endif
