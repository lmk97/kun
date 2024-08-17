#ifndef KUN_WIN_PATH_H
#define KUN_WIN_PATH_H

#include "util/constants.h"

#ifdef KUN_PLATFORM_WIN32

#include "util/bstring.h"
#include "util/result.h"

namespace KUN_SYS {

BString dirname(const BString& path);

BString normalizePath(const BString& path);

inline bool isPathSep(char c) {
    return c == '\\' || c == '/';
}

inline bool isAbspath(const BString& path) {
    return path.contains(":");
}

}

#endif

#endif
