#ifndef KUN_SYS_PATH_H
#define KUN_SYS_PATH_H

#include "util/constants.h"
#include "util/bstring.h"
#include "util/result.h"

#if defined(KUN_PLATFORM_UNIX)

#else
#include "win/path.h"
#endif

namespace kun::sys {

inline BString normalizePath(const BString& path) {
    return KUN_SYS::normalizePath(path);
}

}

#endif
