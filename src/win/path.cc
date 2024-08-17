#include "win/path.h"

#ifdef KUN_PLATFORM_WIN32

#include <stddef.h>

#include <vector>

namespace KUN_SYS {

BString dirname(const BString& path) {
    return "";
}

BString normalizePath(const BString& path) {
    const char* s = path.data();
    auto len = path.length();
    bool isUnc = false;
    size_t i = 0;
    if (path.startsWith("\\\\?\\")) {
        isUnc = true;
        i = 4;
    }
    size_t prev = i;
    std::vector<BString> names;
    names.reserve(32);
    while (i <= len) {
        if (i == len || isPathSep(s[i])) {
            auto name = path.substring(prev, i);
            prev = i + 1;
            if (name == "..") {
                if (names.empty()) {
                    names.emplace_back(name);
                } else {
                    const auto& last = names.back();
                    if (last == "..") {
                        names.emplace_back(name);
                    } else if (!last.contains(":")) {
                        names.pop_back();
                    }
                }
            } else if (!name.empty() && name != ".") {
                names.emplace_back(name);
            }
        }
        i++;
    }
    BString result;
    result.reserve(len);
    if (isUnc) {
        result += "\\\\?\\";
    }
    for (const auto& name : names) {
        result += name;
        result += "\\";
    }
    len = result.length();
    if (len > 1 && result[len - 2] != ':') {
        result.resize(len - 1);
    }
    return result;
}

}

#endif
