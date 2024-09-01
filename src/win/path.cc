#include "win/path.h"

#ifdef KUN_PLATFORM_WIN32

#include <ctype.h>
#include <shlwapi.h>
#include <stddef.h>
#include <windows.h>

#include <vector>

#include "sys/process.h"
#include "win/util.h"

using kun::BString;

namespace {

inline bool isLongPath(const BString& path) {
    return path.startsWith("\\\\?\\") || path.startsWith("\\\\.\\");
}

}

namespace KUN_SYS {

BString dirname(const BString& path) {
    auto cpath = cleanPath(path);
    auto len = cpath.length();
    if (len == 0) {
        return ".";
    }
    const char* begin = cpath.data();
    auto end = begin + len;
    auto s = begin;
    if (isLongPath(cpath)) {
        s += 4;
    }
    if (s + 1 < end && *(s + 1) == ':') {
        s += 2;
    }
    auto p = end - 1;
    while (p > s) {
        if (*p != '\\' || p + 1 == end) {
            p--;
            continue;
        }
        cpath.resize(p - begin);
        return cpath;
    }
    if (*p == '\\') {
        cpath.resize(p + 1 - begin);
        return cpath;
    }
    auto i = cpath.find(":");
    if (i != BString::END) {
        cpath.resize(i + 1);
        return cpath;
    }
    return ".";
}

BString basename(const BString& path) {
    auto cpath = cleanPath(path);
    auto len = cpath.length();
    if (len == 0) {
        return "";
    }
    const char* begin = cpath.data();
    auto end = begin + len;
    auto s = begin;
    if (isLongPath(cpath)) {
        s += 4;
    }
    if (s + 1 < end && *(s + 1) == ':') {
        s += 2;
    }
    auto p = end - 1;
    while (p >= s) {
        if (*p == '\\') {
            return BString(p + 1, end - (p + 1));
        }
        p--;
    }
    auto i = cpath.find(":");
    if (i == BString::END) {
        return cpath;
    }
    return BString(begin + i + 1, len - (i + 1));
}

BString cleanPath(const BString& path) {
    BString prefix;
    BString drive;
    auto p = path.data();
    auto end = p + path.length();
    if (isLongPath(path)) {
        prefix = BString::view(p, 4);
        p += 4;
    }
    if (p + 1 < end && *(p + 1) == ':') {
        drive = BString::view(p, 2);
        p += 2;
    }
    auto prev = p;
    std::vector<BString> names;
    names.reserve(32);
    if (p < end && isPathSeparator(*p)) {
        names.emplace_back("\\");
        prev = ++p;
    }
    while (p <= end) {
        if (p != end && !isPathSeparator(*p)) {
            p++;
            continue;
        }
        auto name = BString::view(prev, p - prev);
        prev = ++p;
        if (name.empty() || isPathSeparator(name)) {
            continue;
        }
        if (name == "..") {
            if (names.empty()) {
                names.emplace_back(std::move(name));
            } else {
                const auto& last = names.back();
                if (last == "..") {
                    names.emplace_back(std::move(name));
                } else if (last != "\\") {
                    names.pop_back();
                }
            }
        } else if (name != ".") {
            names.emplace_back(std::move(name));
        }
    }
    BString result;
    result.reserve(path.length());
    result += prefix;
    result += drive;
    auto size = names.size();
    decltype(size) i = 0;
    for (const auto& name : names) {
        result += name;
        if ((i > 0 && i + 1 < size) ||
            (i + 1 < size && name != "\\")
        ) {
            result += "\\";
        }
        i++;
    }
    if (result == drive) {
        result += ".";
    }
    return result;
}

bool isAbsolutePath(const BString& path) {
    auto i = path.find(":");
    if (i == BString::END ||
        i + 1 == path.length() ||
        !isPathSeparator(path[i + 1])
    ) {
        return false;
    }
    size_t j = 0;
    if (isLongPath(path)) {
        j = 4;
    }
    return j + 1 == i && isalpha(path[j]);
}

bool pathExists(const BString& path) {
    auto result = toWString(path);
    if (!result) {
        return false;
    }
    auto wstr = result.unwrap();
    return ::PathFileExistsW(wstr.c_str()) == TRUE;
}

}

#endif
