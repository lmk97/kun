#ifndef KUN_WIN_UTILS_H
#define KUN_WIN_UTILS_H

#include "util/constants.h"

#ifdef KUN_PLATFORM_WIN32

#include <stddef.h>
#include <wchar.h>
#include <windows.h>

#include "util/bstring.h"
#include "util/result.h"
#include "util/sys_err.h"
#include "util/wstring.h"
#include "win/err.h"

#include "sys/io.h"

namespace KUN_SYS {

inline Result<WString> toWString(const BString& str) {
    if (str.length() == 0) {
        return L"";
    }
    int capacity = ::MultiByteToWideChar(
        CP_UTF8,
        0,
        str.data(),
        static_cast<int>(str.length()),
        nullptr,
        0
    );
    if (capacity == 0) {
        auto errCode = convertError(::GetLastError());
        return SysErr(errCode);
    }
    WString result;
    result.reserve(capacity);
    int nchars = ::MultiByteToWideChar(
        CP_UTF8,
        0,
        str.data(),
        static_cast<int>(str.length()),
        result.data(),
        capacity
    );
    if (nchars == 0) {
        auto errCode = convertError(::GetLastError());
        return SysErr(errCode);
    }
    result.resize(nchars);
    return result;
}

inline Result<BString> toBString(const wchar_t* s, size_t len) {
    if (s == nullptr || len == 0) {
        return "";
    }
    int capacity = ::WideCharToMultiByte(
        CP_UTF8,
        0,
        s,
        static_cast<int>(len),
        nullptr,
        0,
        nullptr,
        nullptr
    );
    if (capacity == 0) {
        auto errCode = convertError(::GetLastError());
        return SysErr(errCode);
    }
    BString result;
    result.reserve(capacity);
    int nbytes = ::WideCharToMultiByte(
        CP_UTF8,
        0,
        s,
        static_cast<int>(len),
        result.data(),
        capacity,
        nullptr,
        nullptr
    );
    if (nbytes == 0) {
        auto errCode = convertError(::GetLastError());
        return SysErr(errCode);
    }
    result.resize(nbytes);
    return result;
}

inline Result<BString> toBString(const WString& str) {
    return toBString(str.data(), str.length());
}

}

#endif

#endif
