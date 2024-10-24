#include "unix/fs.h"

#ifdef KUN_PLATFORM_UNIX

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stddef.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#include <vector>

#include "sys/path.h"
#include "util/scope_guard.h"
#include "util/sys_err.h"
#include "util/utils.h"

using kun::sys::joinPath;

namespace KUN_SYS {

Result<BString> readFile(const BString& path) {
    auto fd = ::open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        return SysErr(errno);
    }
    ON_SCOPE_EXIT {
        if (::close(fd) == -1) {
            KUN_LOG_ERR(errno);
        }
    };
    struct stat st;
    if (::fstat(fd, &st) == -1) {
        return SysErr(errno);
    }
    if (!S_ISREG(st.st_mode)) {
        return SysErr(SysErr::NOT_REGULAR_FILE);
    }
    auto nbytes = st.st_size;
    BString result;
    if (nbytes <= 0) {
        return result;
    }
    result.reserve(nbytes);
    char* begin = result.data();
    auto end = begin + nbytes;
    auto p = begin;
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
    result.resize(nbytes);
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
        if (p == end || *p == '/') {
            dirPath.append(prev, p - prev);
            prev = p;
            if (
                ::access(dirPath.c_str(), F_OK) == -1 &&
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
    std::vector<BString> dirs;
    dirs.reserve(128);
    dirs.emplace_back(BString::view(path));
    size_t index = 0;
    while (true) {
        if (index >= dirs.size()) {
            break;
        }
        const auto& dir = dirs[index++];
        DIR* dp = ::opendir(dir.c_str());
        if (dp == nullptr) {
            return SysErr(errno);
        }
        ON_SCOPE_EXIT {
            if (::closedir(dp) == -1) {
                KUN_LOG_ERR(errno);
            }
        };
        struct dirent* de = nullptr;
        while ((de = ::readdir(dp)) != nullptr) {
            auto name = BString::view(de->d_name, strlen(de->d_name));
            if (name == "." || name == "..") {
                continue;
            }
            auto filePath = joinPath(dir, name);
            struct stat st;
            if (::stat(filePath.c_str(), &st) == -1) {
                return SysErr(errno);
            }
            if (S_ISDIR(st.st_mode)) {
                dirs.emplace_back(std::move(filePath));
            } else if (S_ISREG(st.st_mode)) {
                if (::unlink(filePath.c_str()) == -1) {
                    return SysErr(errno);
                }
            }
        }
    }
    for (auto iter = dirs.crbegin(); iter != dirs.crend(); ++iter) {
        const auto& dir = *iter;
        if (::rmdir(dir.c_str()) == -1) {
            return SysErr(errno);
        }
    }
    return true;
}

}

#endif
