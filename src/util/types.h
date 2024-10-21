#ifndef KUN_UTIL_TYPES_H
#define KUN_UTIL_TYPES_H

#include "util/constants.h"

#if defined(KUN_PLATFORM_WIN32)
#include <winsock2.h>
#endif

#if defined(KUN_PLATFORM_UNIX)
#define KUN_FD_TYPE int
#define KUN_INVALID_FD -1
#elif defined(KUN_PLATFORM_WIN32)
#define KUN_FD_TYPE SOCKET
#define KUN_INVALID_FD INVALID_SOCKET
#endif

#endif
