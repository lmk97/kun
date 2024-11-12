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
    return KUN_SYS::microsecond();
}

inline Result<uint64_t> millisecond() {
    auto result = KUN_SYS::microsecond();
    if (result) {
        auto us = result.unwrap();
        return us / 1000;
    }
    return result.err();
}

}

#endif
