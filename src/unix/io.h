#ifndef KUN_UNIX_IO_H
#define KUN_UNIX_IO_H

#include "util/constants.h"

#ifdef KUN_PLATFORM_UNIX

#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <fcntl.h>

#include "util/bstring.h"
#include "util/result.h"

namespace KUN_SYS {

template<typename... TS>
inline void fprint(FILE* stream, const BString& fmt, TS&&... args) {
    BString str;
    if constexpr (sizeof...(args) == 0) {
        str = BString::view(fmt);
    } else {
        str = BString::format(fmt, std::forward<TS>(args)...);
    }
    int fd = ::fileno(stream);
    const char* p = str.data();
    auto end = p + str.length();
    while (true) {
        size_t len = end - p;
        auto rc = ::write(fd, p, len);
        if (rc > 0 && static_cast<size_t>(rc) < len) {
            p += rc;
            continue;
        }
        if (rc == -1 && (errno == EAGAIN || errno == EINTR)) {
            continue;
        }
        break;
    }
}

inline Result<bool> setNonblocking(int fd) {
    int flag = ::fcntl(fd, F_GETFL);
    if (flag >= 0) {
        flag |= O_NONBLOCK;
        if (::fcntl(fd, F_SETFL, flag) != -1) {
            return true;
        }
    }
    return SysErr(errno);
}

}

#endif

#endif
