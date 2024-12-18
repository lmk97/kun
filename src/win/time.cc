#include "win/time.h"

#ifdef KUN_PLATFORM_WIN32

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

Result<uint64_t> microsecond() {
    static auto frequency = getFrequency().unwrap();
    LARGE_INTEGER value;
    if (::QueryPerformanceCounter(&value) != 0) {
        auto ticks = static_cast<uint64_t>(value.QuadPart);
        auto us = ticks * 1000000 / frequency;
        return us;
    }
    auto errCode = convertError(::GetLastError());
    return SysErr(errCode);
}

Result<struct timespec> nanosecond() {
    auto result = microsecond();
    if (result) {
        auto us = result.unwrap();
        auto s = us / 1000000;
        auto ns = (us - s * 1000000) * 1000;
        struct timespec ts;
        ts.tv_sec = static_cast<time_t>(s);
        ts.tv_nsec = static_cast<long>(ns);
        return ts;
    }
    return result.err();
}

}

#endif
