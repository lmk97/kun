#ifndef KUN_WIN_PATH_H
#define KUN_WIN_PATH_H

#include "util/constants.h"

#ifdef KUN_PLATFORM_WIN32

#include "util/bstring.h"
#include "util/result.h"

namespace KUN_SYS {

BString dirname(const BString& path);

BString basename(const BString& path);

BString cleanPath(const BString& path);

bool isAbsolutePath(const BString& path);

bool pathExists(const BString& path);

inline bool isPathSeparator(char c) {
    return c == '\\' || c == '/';
}

inline bool isPathSeparator(const BString& path) {
    return path.length() == 1 && isPathSeparator(path[0]);
}

}

#endif

#endif
