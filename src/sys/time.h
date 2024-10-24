#ifndef KUN_SYS_TIME_H
#define KUN_SYS_TIME_H

#include "util/constants.h"

#if defined(KUN_PLATFORM_UNIX)
#include "unix/time.h"
#elif defined(KUN_PLATFORM_WIN32)
#include "win/time.h"
#endif

namespace kun::sys {

inline Result<struct timespec> nanosecond() {
    return KUN_SYS::nanosecond();
}

inline Result<uint64_t> microsecond() {
    auto result = KUN_SYS::nanosecond();
    if (result) {
        auto ts = result.unwrap();
        uint64_t s = ts.tv_sec;
        uint64_t ns = ts.tv_nsec;
        return s * 1000000 + ns / 1000;
    }
    return result.err();
}

inline Result<uint64_t> millisecond() {
    auto result = KUN_SYS::nanosecond();
    if (result) {
        auto ts = result.unwrap();
        uint64_t s = ts.tv_sec;
        uint64_t ns = ts.tv_nsec;
        return s * 1000 + ns / 1000000;
    }
    return result.err();
}

}

#endif
