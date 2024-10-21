#ifndef KUN_SYS_IO_H
#define KUN_SYS_IO_H

#include "util/constants.h"
#include "util/types.h"

#if defined(KUN_PLATFORM_UNIX)
#include "unix/io.h"
#elif defined(KUN_PLATFORM_WIN32)
#include "win/io.h"
#endif

namespace kun::sys {

template<typename... TS>
inline void print(const BString& fmt, TS&&... args) {
    KUN_SYS::fprint<1>(fmt, std::forward<TS>(args)...);
}

template<typename... TS>
inline void println(const BString& fmt, TS&&... args) {
    KUN_SYS::fprint<1>(fmt, std::forward<TS>(args)...);
    KUN_SYS::fprint<1>("\n");
}

template<typename... TS>
inline void eprint(const BString& fmt, TS&&... args) {
    KUN_SYS::fprint<2>(fmt, std::forward<TS>(args)...);
}

template<typename... TS>
inline void eprintln(const BString& fmt, TS&&... args) {
    KUN_SYS::fprint<2>(fmt, std::forward<TS>(args)...);
    KUN_SYS::fprint<2>("\n");
}

inline Result<bool> setNonblocking(KUN_FD_TYPE fd) {
    return KUN_SYS::setNonblocking(fd);
}

}

#endif
