#include "unix/process.h"

#ifdef KUN_PLATFORM_UNIX

#include <errno.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#include "util/sys_err.h"

namespace KUN_SYS {

Result<BString> getCwd() {
    BString result;
    result.reserve(PATH_MAX);
    char* buf = result.data();
    if (::getcwd(buf, PATH_MAX) == nullptr) {
        return SysErr(errno);
    }
    result.resize(strlen(buf));
    return result;
}

Result<BString> getHomeDir() {
    auto home = ::getenv("HOME");
    if (home == nullptr) {
        return SysErr::err("'HOME' is not found");
    }
    return BString(home, strlen(home));
}

Result<BString> getAppDir() {
    return getHomeDir();
}

}

#endif
