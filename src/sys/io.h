#ifndef KUN_SYS_IO_H
#define KUN_SYS_IO_H

#include "util/constants.h"
#include "util/type_def.h"

#if defined(KUN_PLATFORM_UNIX)
#include "unix/io.h"
#elif defined(KUN_PLATFORM_WIN32)
#include "win/io.h"
#endif

namespace kun::sys {

template<typename... TS>
inline void fprint(FILE* stream, const BString& fmt, TS&&... args) {
    KUN_SYS::fprint(stream, fmt, std::forward<TS>(args)...);
}

template<typename... TS>
inline void fprintln(FILE* stream, const BString& fmt, TS&&... args) {
    fprint(stream, fmt, std::forward<TS>(args)...);
    fprint(stream, "\n");
}

template<typename... TS>
inline void print(const BString& fmt, TS&&... args) {
    fprint(stdout, fmt, std::forward<TS>(args)...);
}

template<typename... TS>
inline void println(const BString& fmt, TS&&... args) {
    fprintln(stdout, fmt, std::forward<TS>(args)...);
}

template<typename... TS>
inline void eprint(const BString& fmt, TS&&... args) {
    fprint(stderr, fmt, std::forward<TS>(args)...);
}

template<typename... TS>
inline void eprintln(const BString& fmt, TS&&... args) {
    fprintln(stderr, fmt, std::forward<TS>(args)...);
}

inline Result<bool> setNonblocking(KUN_FD_TYPE fd) {
    return KUN_SYS::setNonblocking(fd);
}

}

#endif
