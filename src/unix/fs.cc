#include "unix/fs.h"

#ifdef KUN_PLATFORM_UNIX

#include <stddef.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/stat.h>

#include "util/sys_err.h"
#include "util/util.h"
#include "util/scope_guard.h"
#include "sys/io.h"

using kun::util::joinPath;
using kun::sys::eprintln;

namespace KUN_SYS {

Result<BString> readFile(const BString& path) {
    auto fd = ::open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        return SysErr(errno);
    }
    ON_SCOPE_EXIT {
        if (::close(fd) == -1) {
            auto [code, phrase] = SysErr(errno);
            eprintln("ERROR: 'close' ({}) {}", code, phrase);
        }
    };
    struct stat st;
    if (::fstat(fd, &st) == -1) {
        return SysErr(errno);
    }
    if (!S_ISREG(st.st_mode)) {
        return SysErr::err("Not a file");
    }
    auto nbytes = ::lseek(fd, 0, SEEK_END);
    if (nbytes == -1) {
        return SysErr(errno);
    }
    if (::lseek(fd, 0, SEEK_SET) == -1) {
        return SysErr(errno);
    }
    BString result;
    result.reserve(static_cast<size_t>(nbytes));
    char* p = result.data();
    auto end = p + nbytes;
    while (true) {
        size_t len = end - p;
        auto rc = ::read(fd, p, len);
        if (rc > 0 && static_cast<size_t>(rc) < len) {
            p += rc;
            continue;
        }
        if (rc == -1 && (errno == EAGAIN || errno == EINTR)) {
            continue;
        }
        break;
    }
    result.resize(static_cast<size_t>(nbytes));
    return result;
}

Result<bool> makeDirs(const BString& path) {
    auto len = path.length();
    auto begin = path.data();
    auto end = begin + len;
    auto prev = begin;
    auto p = begin + 1;
    BString dirPath;
    dirPath.reserve(len);
    while (p <= end) {
        if (*p == '/' || p == end) {
            dirPath.append(prev, p - prev);
            prev = p;
            if (::access(dirPath.c_str(), F_OK) == -1 &&
                ::mkdir(dirPath.c_str(), 0777) == -1
            ) {
                return SysErr(errno);
            }
        }
        ++p;
    }
    return true;
}

Result<bool> removeDir(const BString& path) {
    DIR* dp = ::opendir(path.c_str());
    if (dp == nullptr) {
        return SysErr(errno);
    }
    ON_SCOPE_EXIT {
        if (::closedir(dp) == -1) {
            auto [code, phrase] = SysErr(errno);
            eprintln("ERROR: 'closedir' ({}) {}", code, phrase);
        }
    };
    struct dirent* de = nullptr;
    struct stat st;
    while ((de = ::readdir(dp)) != nullptr) {
        if (strcmp(de->d_name, ".") == 0 ||
            strcmp(de->d_name, "..") == 0
        ) {
            continue;
        }
        auto name = BString::view(de->d_name, strlen(de->d_name));
        auto filePath = joinPath(path, name);
        if (::stat(filePath.c_str(), &st) == -1) {
            return SysErr(errno);
        }
        if (S_ISREG(st.st_mode)) {
            if (::unlink(filePath.c_str()) == -1) {
                return SysErr(errno);
            }
        } else if (S_ISDIR(st.st_mode)) {
            if (auto result = removeDir(filePath); !result) {
                return result.err();
            }
        }
    }
    if (::rmdir(path.c_str()) == -1) {
        return SysErr(errno);
    }
    return true;
}

}

#endif
