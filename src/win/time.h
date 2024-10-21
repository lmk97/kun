#ifndef KUN_WIN_TIME_H
#define KUN_WIN_TIME_H

#include "util/constants.h"

#ifdef KUN_PLATFORM_WIN32

#include <time.h>

#include "util/result.h"

namespace KUN_SYS {

Result<struct timespec> nanosecond();

}

#endif

#endif
