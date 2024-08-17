#ifndef KUN_WIN_UTIL_H
#define KUN_WIN_UTIL_H

#include "util/constants.h"

#ifdef KUN_PLATFORM_WIN32

#include <stddef.h>
#include <wchar.h>
#include <windows.h>

#include <string>

#include "util/bstring.h"
#include "util/result.h"
#include "util/sys_err.h"
#include "win/err.h"

namespace KUN_SYS {

class WString {
public:
    WString(const wchar_t* s, size_t len) :
        buf(const_cast<wchar_t*>(s)), size(len), kind(HEAP) {}

    template<size_t N>
    WString(const wchar_t (&s)[N]):
        buf(const_cast<wchar_t*>(s)), size(N - 1), kind(VIEW) {}

    WString(const WString&) = delete;

    WString& operator=(const WString&) = delete;

    WString(WString&& wstr) {
        this->buf = wstr.buf;
        this->size = wstr.size;
        this->kind = wstr.kind;
        wstr.buf = nullptr;
        wstr.size = 0;
        wstr.kind = VIEW;
    }

    WString& operator=(WString&& wstr) {
        if (this->kind == HEAP && this->buf != nullptr) {
            delete[] this->buf;
        }
        this->buf = wstr.buf;
        this->size = wstr.size;
        this->kind = wstr.kind;
        wstr.buf = nullptr;
        wstr.size = 0;
        wstr.kind = VIEW;
    }

    ~WString() {
        if (kind == HEAP && buf != nullptr) {
            delete[] buf;
        }
    }

    const wchar_t* data() {
        return buf;
    }

    const wchar_t* data() const {
        return buf;
    }

    const wchar_t* c_str() const {
        return buf;
    }

    size_t length() const {
        return size;
    }

    wchar_t operator[](size_t index) const {
        return buf[index];
    }

    wchar_t& operator[](size_t index) {
        return buf[index];
    }

private:
    wchar_t* buf;
    size_t size;
    int kind;

    static constexpr int HEAP = 0;
    static constexpr int VIEW = 1;
};

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
    auto wstr = new wchar_t[capacity + 1];
    int nchar = ::MultiByteToWideChar(
        CP_UTF8,
        0,
        str.data(),
        static_cast<int>(str.length()),
        wstr,
        capacity
    );
    if (nchar == 0) {
        delete[] wstr;
        auto errCode = convertError(::GetLastError());
        return SysErr(errCode);
    }
    wstr[nchar] = L'\0';
    return WString(wstr, nchar);
}

inline Result<BString> toBString(const wchar_t* s, size_t len) {
    if (len == 0) {
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
    result.reserve(static_cast<size_t>(capacity));
    int nbyte = ::WideCharToMultiByte(
        CP_UTF8,
        0,
        s,
        static_cast<int>(len),
        result.data(),
        capacity,
        nullptr,
        nullptr
    );
    if (nbyte == 0) {
        auto errCode = convertError(::GetLastError());
        return SysErr(errCode);
    }
    result.resize(static_cast<size_t>(nbyte));
    return result;
}

inline Result<BString> toBString(const WString& wstr) {
    return toBString(wstr.data(), wstr.length());
}

inline Result<BString> toBString(const std::wstring& wstr) {
    return toBString(wstr.data(), wstr.length());
}

}

#endif

#endif
