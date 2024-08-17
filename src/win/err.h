#ifndef KUN_WIN_ERR_H
#define KUN_WIN_ERR_H

#include "util/constants.h"

#ifdef KUN_PLATFORM_WIN32

namespace KUN_SYS {

int convertError(int code);

}

#endif

#endif
