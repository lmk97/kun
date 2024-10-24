#include "win/process.h"

#ifdef KUN_PLATFORM_WIN32

#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <userenv.h>
#include <wchar.h>

#include "sys/path.h"
#include "util/scope_guard.h"
#include "util/sys_err.h"
#include "util/utils.h"
#include "util/wstring.h"
#include "win/err.h"
#include "win/utils.h"

using kun::BString;
using kun::Result;
using kun::SysErr;
using kun::WString;
using kun::sys::joinPath;
using kun::sys::pathExists;
using kun::win::toBString;

namespace {

template<size_t N>
inline Result<BString> getEnvVar(const wchar_t (&s)[N]) {
    static_assert(N > 0);
    auto len = ::GetEnvironmentVariable(s, nullptr, 0);
    if (len != 0) {
        WString result;
        result.reserve(len);
        auto nchars = ::GetEnvironmentVariable(s, result.data(), len);
        if (nchars != 0) {
            result.resize(nchars);
            return toBString(result);
        }
    }
    return SysErr::err("env var not found");
}

}

namespace KUN_SYS {

Result<BString> getCwd() {
    auto len = ::GetCurrentDirectory(0, nullptr);
    if (len == 0) {
        auto errCode = convertError(::GetLastError());
        return SysErr(errCode);
    }
    auto buf = new wchar_t[len];
    ON_SCOPE_EXIT {
        delete[] buf;
    };
    auto nchars = ::GetCurrentDirectory(len, buf);
    if (nchars == 0) {
        auto errCode = convertError(::GetLastError());
        return SysErr(errCode);
    }
    return toBString(buf, nchars);
}

Result<BString> getHomeDir() {
    if (auto result = getEnvVar(L"USERPROFILE")) {
        return result.unwrap();
    }
    auto process = ::GetCurrentProcess();
    HANDLE token;
    if (::OpenProcessToken(process, TOKEN_READ, &token) == 0) {
        auto errCode = convertError(::GetLastError());
        return SysErr(errCode);
    }
    ON_SCOPE_EXIT {
        if (::CloseHandle(token) == 0) {
            auto errCode = convertError(::GetLastError());
            KUN_LOG_ERR(errCode);
        }
    };
    DWORD len = 0;
    ::GetUserProfileDirectoryW(token, nullptr, &len);
    if (len == 0) {
        return SysErr::err("home dir not found");
    }
    WString result;
    result.reserve(len);
    if (::GetUserProfileDirectoryW(token, result.data(), &len) == 0) {
        auto errCode = convertError(::GetLastError());
        return SysErr(errCode);
    }
    result.resize(len);
    return toBString(result);
}

Result<BString> getAppDir() {
    if (auto result = getEnvVar(L"LOCALAPPDATA")) {
        return result.unwrap();
    }
    if (auto result = getHomeDir()) {
        auto homeDir = result.unwrap();
        auto appDir = joinPath(homeDir, "AppData/Local");
        if (pathExists(appDir)) {
            return appDir;
        }
        return homeDir;
    } else {
        return result.err();
    }
}

}

#endif
