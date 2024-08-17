#ifndef KUN_UNIX_TIME_H
#define KUN_UNIX_TIME_H

#include "util/constants.h"

#ifdef KUN_PLATFORM_UNIX

#include <stdint.h>
#include <errno.h>
#include <time.h>

#include "util/result.h"

namespace KUN_SYS {

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
