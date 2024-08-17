#ifndef KUN_SYS_PROCESS_H
#define KUN_SYS_PROCESS_H

#include "util/constants.h"

#if defined(KUN_PLATFORM_UNIX)
#include "unix/process.h"
#elif defined(KUN_PLATFORM_WIN32)
#include "win/process.h"
#endif

namespace kun::sys {

inline Result<BString> getCwd() {
    return KUN_SYS::getCwd();
}

}

#endif
