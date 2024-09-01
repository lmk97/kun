#ifndef KUN_UNIX_PATH_H
#define KUN_UNIX_PATH_H

#include "util/constants.h"

#ifdef KUN_PLATFORM_UNIX

#include "util/bstring.h"
#include "util/result.h"

namespace KUN_SYS {

BString dirname(const BString& path);

BString basename(const BString& path);

BString cleanPath(const BString& path);

bool pathExists(const BString& path);

inline bool isPathSeparator(char c) {
    return c == '/';
}

inline bool isPathSeparator(const BString& path) {
    return path.length() == 1 && isPathSeparator(path[0]);
}

inline bool isAbsolutePath(const BString& path) {
    return path.length() > 0 && isPathSeparator(path[0]);
}

}

#endif

#endif
