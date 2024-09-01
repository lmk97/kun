#include "unix/path.h"

#ifdef KUN_PLATFORM_UNIX

#include <unistd.h>

#include <vector>

namespace KUN_SYS {

BString dirname(const BString& path) {
    auto cpath = cleanPath(path);
    const char* begin = cpath.data();
    auto end = begin + cpath.length();
    auto p = end - 1;
    while (p > begin) {
        if (*p != '/' || p + 1 == end) {
            p--;
            continue;
        }
        cpath.resize(p - begin);
        return cpath;
    }
    if (*p == '/') {
        cpath.resize(p + 1 - begin);
        return cpath;
    }
    return ".";
}

BString basename(const BString& path) {
    auto cpath = cleanPath(path);
    const char* begin = cpath.data();
    auto end = begin + cpath.length();
    auto p = end - 1;
    while (p >= begin) {
        if (*p == '/') {
            return BString(p + 1, end - (p + 1));
        }
        p--;
    }
    return cpath;
}

BString cleanPath(const BString& path) {
    auto p = path.data();
    auto end = p + path.length();
    auto prev = p;
    std::vector<BString> names;
    names.reserve(32);
    if (p < end && isPathSeparator(*p)) {
        names.emplace_back("/");
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
                } else if (last != "/") {
                    names.pop_back();
                }
            }
        } else if (name != ".") {
            names.emplace_back(std::move(name));
        }
    }
    BString result;
    result.reserve(path.length());
    auto size = names.size();
    decltype(size) i = 0;
    for (const auto& name : names) {
        result += name;
        if ((i > 0 && i + 1 < size) ||
            (i + 1 < size && name != "/")
        ) {
            result += "/";
        }
        i++;
    }
    return result;
}

bool pathExists(const BString& path) {
    return ::access(path.c_str(), F_OK) == 0;
}

}

#endif
