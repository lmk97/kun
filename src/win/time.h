#ifndef KUN_WIN_TIME_H
#define KUN_WIN_TIME_H

#include "util/constants.h"

#ifdef KUN_PLATFORM_WIN32

#include <stdint.h>
#include <time.h>
#include <windows.h>

#include "util/result.h"
#include "util/sys_err.h"
#include "win/err.h"

namespace KUN_SYS {

inline Result<struct timespec> nanosecond() {
    static int64_t frequency = 0;
    if (frequency == 0) {
        LARGE_INTEGER value;
        if (::QueryPerformanceFrequency(&value)) {
            frequency = static_cast<int64_t>(value.QuadPart);
        } else {
            auto errCode = convertError(::GetLastError());
            return SysErr(errCode);
        }
    }
    LARGE_INTEGER value;
    if (::QueryPerformanceCounter(&value)) {
        auto ticks = static_cast<int64_t>(value.QuadPart);
        auto t = ticks * 10000000 / frequency;
        auto s = t / 10000000;
        auto d = t - s * 10000000;
        auto ns = d * 100;
        struct timespec ts;
        ts.tv_sec = static_cast<time_t>(s);
        ts.tv_nsec = static_cast<long>(ns);
        return ts;
    }
    auto errCode = convertError(::GetLastError());
    return SysErr(errCode);
}

}

#endif

#endif
