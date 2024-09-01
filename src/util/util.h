#ifndef KUN_UTIL_UTIL_H
#define KUN_UTIL_UTIL_H

#include "sys/io.h"

#define KUN_LOG_ERR(arg) \
    kun::util::logErr(arg); \
    kun::sys::eprintln( \
        "    at \033[0;36m{}\033[0m:\033[0;33m{}\033[0m", \
        __FILE__, __LINE__ \
    )

namespace kun::util {

template<typename T>
inline void logErr(T&& t) {
    if constexpr (std::is_same_v<std::decay_t<T>, int>) {
        sys::eprintln(
            "\033[0;31mERROR\033[0m: (\033[0;33m{}\033[0m) {}",
            t, SysErr(t).phrase
        );
    } else {
        sys::eprintln("\033[0;31mERROR\033[0m: {}", t);
    }
}

}

#endif
