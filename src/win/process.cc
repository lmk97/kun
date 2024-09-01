#include "win/process.h"

#ifdef KUN_PLATFORM_WIN32

#include <stdlib.h>
#include <string.h>

#include "util/scope_guard.h"
#include "util/sys_err.h"
#include "win/err.h"
#include "win/util.h"

namespace KUN_SYS {

Result<BString> getCwd() {
    auto len = ::GetCurrentDirectory(0, nullptr);
    if (len == 0) {
        auto errCode = convertError(::GetLastError());
        return SysErr(errCode);
    }
    auto buf = new TCHAR[len];
    ON_SCOPE_EXIT {
        delete[] buf;
    };
    auto nchars = ::GetCurrentDirectory(len, buf);
    if (nchars == 0) {
        auto errCode = convertError(::GetLastError());
        return SysErr(errCode);
    }
    if (auto result = toBString(buf, nchars)) {
        return result.unwrap();
    } else {
        return result.err();
    }
}

Result<BString> getHomeDir() {
    wchar_t* drive = nullptr;
    size_t driveLen;
    if (::_wdupenv_s(&drive, &driveLen, L"HOMEDRIVE") != 0) {
        return SysErr::err("'HOMEDRIVE' is not found");
    }
    wchar_t* path = nullptr;
    size_t pathLen;
    ON_SCOPE_EXIT {
        free(drive);
        free(path);
    };
    if (::_wdupenv_s(&path, &pathLen, L"HOMEPATH") != 0) {
        return SysErr::err("'HOMEPATH' is not found");
    }
    BString result;
    result.reserve(driveLen + pathLen);
    if (auto r = toBString(drive, driveLen)) {
        result += r.unwrap();
    } else {
        return r.err();
    }
    if (auto r = toBString(path, pathLen)) {
        result += r.unwrap();
    } else {
        return r.err();
    }
    return result;
}

Result<BString> getAppDir() {
    wchar_t* dir = nullptr;
    size_t dirLen;
    if (::_wdupenv_s(&dir, &dirLen, L"LOCALAPPDATA") != 0) {
        return SysErr::err("'LOCALAPPDATA' is not found");
    }
    ON_SCOPE_EXIT {
        free(dir);
    };
    return toBString(dir, dirLen);
}

}

#endif
