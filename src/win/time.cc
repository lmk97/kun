#include "win/time.h"

#ifdef KUN_PLATFORM_WIN32

#include <stdint.h>
#include <windows.h>

#include "util/sys_err.h"
#include "win/err.h"

using kun::Result;
using kun::SysErr;
using kun::win::convertError;

namespace {

inline Result<uint64_t> getFrequency() {
    LARGE_INTEGER value;
    if (::QueryPerformanceFrequency(&value) != 0) {
        auto freq = static_cast<uint64_t>(value.QuadPart);
        return freq;
    }
    auto errCode = convertError(::GetLastError());
    return SysErr(errCode);
}

}

namespace KUN_SYS {

Result<struct timespec> nanosecond() {
    static auto frequency = getFrequency().unwrap();
    LARGE_INTEGER value;
    if (::QueryPerformanceCounter(&value) != 0) {
        auto ticks = static_cast<uint64_t>(value.QuadPart);
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
