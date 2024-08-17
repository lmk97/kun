#include "util/util.h"

#include <string.h>
#include <limits.h>

#include <vector>

#include "util/scope_guard.h"
#include "sys/process.h"

#define KUN_PRIVATE_DIRNAME \
    auto i = path.rfind("/"); \
    if (i == kun::BString::END) { \
        return "."; \
    } \
    if (i + 1 == path.length()) { \
        i = path.rfind("/", i - 1); \
    } \
    if (i == kun::BString::END) { \
        return "."; \
    }

#define KUN_PRIVATE_TO_ABSOLUTE_PATH(ret) \
    if (path[0] == '/') { \
        return ret; \
    } \
    auto cwd = kun::sys::getCwd().unwrap(); \
    auto tmpPath = kun::util::joinPath(cwd, path); \
    return kun::util::normalizePath(tmpPath)

namespace kun::util {

BString dirname(const BString& path) {
    KUN_PRIVATE_DIRNAME
    return i > 0 ? path.substring(0, i) : "/";
}

BString dirname(BString&& path) {
    KUN_PRIVATE_DIRNAME
    path.resize(i);
    return std::move(path);
}

BString normalizePath(const BString& path) {
    std::vector<size_t> indexes;
    indexes.reserve(32);
    auto len = path.length();
    const char* begin = path.data();
    const char* end = begin + len;
    const char* prev = begin;
    const char* p = begin;
    while (p < end) {
        if (*p == '/') {
            size_t n = p - prev;
            if ((n == 2 && memcmp(prev, "..", 2) == 0) ||
                (n == 3 && memcmp(prev, "/..", 3) == 0)
            ) {
                if (!indexes.empty()) {
                    indexes.pop_back();
                    indexes.pop_back();
                }
            } else if ((n == 1 && memcmp(prev, ".", 1) != 0) ||
                (n == 2 && memcmp(prev, "/.", 2) != 0) ||
                (n > 2)
            ) {
                indexes.push_back(prev - begin);
                indexes.push_back(p - begin);
            }
            prev = p;
        }
        ++p;
    }
    size_t n = end - prev;
    if (n == 3 && memcmp(prev, "/..", 3) == 0) {
        if (!indexes.empty()) {
            indexes.pop_back();
            indexes.pop_back();
        }
    } else if ((n == 2 && memcmp(prev, "/.", 2) != 0) || n > 2) {
        indexes.push_back(prev - begin);
        indexes.push_back(len);
    }
    BString str;
    str.reserve(len);
    for (auto iter = indexes.begin(); iter != indexes.end(); iter += 2) {
        str += path.substring(*iter, *(iter + 1));
    }
    return str;
}

BString joinPath(const BString& dir, const BString& path) {
    auto dirLen = dir.length();
    BString str;
    str.reserve(dirLen + path.length() + 1);
    if (dir[dirLen - 1] == '/' && path[0] == '/') {
        str += dir.substring(0, dirLen - 1);
        str += path;
    } else if (dir[dirLen - 1] != '/' && path[0] != '/') {
        str += dir;
        str += "/";
        str += path;
    } else {
        str += dir;
        str += path;
    }
    return str;
}

BString toAbsolutePath(const BString& path) {
    KUN_PRIVATE_TO_ABSOLUTE_PATH(BString::view(path));
}

BString toAbsolutePath(BString&& path) {
    KUN_PRIVATE_TO_ABSOLUTE_PATH(std::move(path));
}

}
