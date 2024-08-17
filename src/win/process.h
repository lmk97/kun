#ifndef KUN_WIN_PROCESS_H
#define KUN_WIN_PROCESS_H

#include "util/constants.h"

#ifdef KUN_PLATFORM_WIN32

#include "util/bstring.h"
#include "util/result.h"

namespace KUN_SYS {

Result<BString> getCwd();

}

#endif

#endif
