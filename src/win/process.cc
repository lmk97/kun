#include "win/process.h"

#ifdef KUN_PLATFORM_WIN32

#include "util/sys_err.h"
#include "util/scope_guard.h"
#include "win/err.h"
#include "win/util.h"

namespace KUN_SYS {

Result<BString> getCwd() {
    auto len = ::GetCurrentDirectory(0, nullptr);
    if (len == 0) {
        auto errCode = convertError(::GetLastError());
        return SysErr(errCode);
    }
    auto buf = new TCHAR[len + 1];
    ON_SCOPE_EXIT {
        delete[] buf;
    };
    auto bufLen = ::GetCurrentDirectory(len + 1, buf);
    if (bufLen == 0) {
        auto errCode = convertError(::GetLastError());
        return SysErr(errCode);
    }
    if (auto result = toBString(buf, bufLen)) {
        return result.unwrap();
    } else {
        return result.err();
    }
}

}

#endif
