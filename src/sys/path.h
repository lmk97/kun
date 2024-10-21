#ifndef KUN_SYS_PATH_H
#define KUN_SYS_PATH_H

#include <string.h>

#include "sys/process.h"
#include "util/constants.h"
#include "util/traits.h"

#if defined(KUN_PLATFORM_UNIX)
#include "unix/path.h"
#else
#include "win/path.h"
#endif

namespace kun::sys {

inline BString dirname(const BString& path) {
    return KUN_SYS::dirname(path);
}

inline BString basename(const BString& path) {
    return KUN_SYS::basename(path);
}

inline BString cleanPath(const BString& path) {
    return KUN_SYS::cleanPath(path);
}

inline bool isAbsolutePath(const BString& path) {
    return KUN_SYS::isAbsolutePath(path);
}

inline bool isPathSeparator(char c) {
    return KUN_SYS::isPathSeparator(c);
}

inline bool isPathSeparator(const BString& path) {
    return KUN_SYS::isPathSeparator(path);
}

inline bool pathExists(const BString& path) {
    return KUN_SYS::pathExists(path);
}

template<typename... TS>
inline BString joinPath(const BString& path, TS&&... paths) {
    auto capacity = path.length();
    ([&](auto&& t) {
        using T = typename std::decay_t<decltype(t)>;
        static_assert(
            std::is_same_v<T, BString> ||
            kun::is_c_str<T>
        );
        if constexpr (std::is_same_v<T, BString>) {
            capacity += 1 + t.length();
        } else {
            capacity += 1 + strlen(t);
        }
    }(std::forward<TS>(paths)), ...);
    BString result;
    result.reserve(capacity);
    result += path;
    ([&](auto&& t) {
        result += KUN_PATH_SEPARATOR;
        using T = typename std::decay_t<decltype(t)>;
        if constexpr (std::is_same_v<T, BString>) {
            result += t;
        } else {
            result.append(t, strlen(t));
        }
    }(std::forward<TS>(paths)), ...);
    return cleanPath(result);
}

inline Result<BString> toAbsolutePath(const BString& path) {
    if (isAbsolutePath(path)) {
        return cleanPath(path);
    }
    if (auto result = getCwd()) {
        auto cwd = result.unwrap();
        return joinPath(cwd, path);
    } else {
        return result.err();
    }
}

}

#endif
