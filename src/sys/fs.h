#ifndef KUN_SYS_FS_H
#define KUN_SYS_FS_H

#include "util/constants.h"

#if defined(KUN_PLATFORM_UNIX)
#include "unix/fs.h"
#elif defined(KUN_PLATFORM_WIN32)
#include "win/fs.h"
#endif

namespace kun::sys {

inline Result<BString> readFile(const BString& path) {
    return KUN_SYS::readFile(path);
}

inline Result<bool> makeDirs(const BString& path) {
    return KUN_SYS::makeDirs(path);
}

inline Result<bool> removeDir(const BString& path) {
    return KUN_SYS::removeDir(path);
}

}

#endif
