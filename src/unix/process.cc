#include "unix/process.h"

#ifdef KUN_PLATFORM_UNIX

#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "util/scope_guard.h"
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
    if (home != nullptr) {
        return BString(home, strlen(home));
    }
    size_t capacity;
    auto size = ::sysconf(_SC_GETPW_R_SIZE_MAX);
    if (size == -1) {
        capacity = PATH_MAX;
    } else {
        capacity = size;
    }
    auto buf = new char[capacity];
    ON_SCOPE_EXIT {
        delete[] buf;
    };
    auto uid = ::getuid();
    struct passwd pw;
    struct passwd* result = nullptr;
    while (true) {
        auto rc = ::getpwuid_r(uid, &pw, buf, capacity, &result);
        if (rc == 0) {
            break;
        }
        if (rc == ERANGE) {
            delete[] buf;
            capacity = capacity << 1;
            buf = new char[capacity];
        } else {
            if (rc == EINTR) {
                continue;
            }
            break;
        }
    }
    if (result == nullptr) {
        return SysErr::err("'HOME' is not found");
    }
    return BString(pw.pw_dir, strlen(pw.pw_dir));
}

Result<BString> getAppDir() {
    return getHomeDir();
}

}

#endif
