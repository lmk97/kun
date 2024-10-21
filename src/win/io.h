#ifndef KUN_WIN_IO_H
#define KUN_WIN_IO_H

#include "util/constants.h"

#ifdef KUN_PLATFORM_WIN32

#include <stddef.h>
#include <windows.h>

#include "util/bstring.h"
#include "util/result.h"
#include "win/err.h"

namespace KUN_SYS {

template<int N, typename... TS>
inline void fprint(const BString& fmt, TS&&... args) {
    static_assert(N == 1 || N == 2);
    BString str;
    if constexpr (sizeof...(args) == 0) {
        str = BString::view(fmt);
    } else {
        str = BString::format(fmt, std::forward<TS>(args)...);
    }
    constexpr auto n = N == 1 ? STD_OUTPUT_HANDLE : STD_ERROR_HANDLE;
    auto handle = ::GetStdHandle(n);
    const char* p = str.data();
    auto end = p + str.length();
    DWORD nbytes = 0;
    while (p < end) {
        auto len = static_cast<DWORD>(end - p);
        ::WriteFile(handle, p, len, &nbytes, nullptr);
        p += nbytes;
        nbytes = 0;
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
