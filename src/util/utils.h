#ifndef KUN_UTIL_UTILS_H
#define KUN_UTIL_UTILS_H

#include "sys/io.h"
#include "util/bstring.h"

#define KUN_LOG_ERR(...) \
    kun::util::logErr(__VA_ARGS__); \
    kun::sys::eprintln( \
        "    at \x1b[0;36m{}\x1b[0m:\x1b[0;33m{}\x1b[0m", \
        __FILE__, __LINE__ \
    )

namespace kun::util {

template<typename T, typename... TS>
inline void logErr(T&& t, TS&&... args) {
    if constexpr (std::is_same_v<std::decay_t<T>, int>) {
        auto [code, name, phrase] = SysErr(t);
        sys::eprintln(
            "\x1b[0;31mERROR\x1b[0m: {}(\x1b[0;33m{}\x1b[0m) {}",
            name, code, phrase
        );
    } else {
        auto str = BString::format(t, std::forward<TS>(args)...);
        sys::eprintln("\x1b[0;31mERROR\x1b[0m: {}", str);
    }
}

}

#endif
