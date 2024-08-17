#include "unix/process.h"

#ifdef KUN_PLATFORM_UNIX

#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <limits.h>

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

}

#endif
