#ifndef KUN_WIN_IO_H
#define KUN_WIN_IO_H

#include "util/constants.h"

#ifdef KUN_PLATFORM_WIN32

#include <stdio.h>
#include <stddef.h>
#include <windows.h>

#include "util/bstring.h"
#include "util/result.h"
#include "win/err.h"

namespace KUN_SYS {

template<typename... TS>
inline void fprint(FILE* stream, const BString& fmt, TS&&... args) {
    BString str;
    if constexpr (sizeof...(args) == 0) {
        str = BString::view(fmt);
    } else {
        str = BString::format(fmt, std::forward<TS>(args)...);
    }
    auto nStdHandle = stream == stdout ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE;
    auto handle = ::GetStdHandle(nStdHandle);
    auto p = str.c_str();
    auto end = p + str.length();
    DWORD nbyte = 0;
    while (p < end) {
        ::WriteFile(handle, p, static_cast<DWORD>(end - p), &nbyte, nullptr);
        p += nbyte;
        nbyte = 0;
    }
}

inline Result<bool> setNonblocking(SOCKET sock) {
    u_long flag = 1;
    if (::ioctlsocket(sock, FIONBIO, &flag) == 0) {
        return true;
    }
    auto errCode = convertError(::WSAGetLastError());
    return SysErr(errCode);
}

}

#endif

#endif
