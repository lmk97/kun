#ifndef KUN_UNIX_PROCESS_H
#define KUN_UNIX_PROCESS_H

#include "util/constants.h"

#ifdef KUN_PLATFORM_UNIX

#include "util/bstring.h"
#include "util/result.h"

namespace KUN_SYS {

Result<BString> getCwd();

Result<BString> getHomeDir();

Result<BString> getAppDir();

}

#endif

#endif
