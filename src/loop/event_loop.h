#ifndef KUN_LOOP_EVENT_LOOP_H
#define KUN_LOOP_EVENT_LOOP_H

#include "util/constants.h"

#if defined(KUN_PLATFORM_LINUX)
#include "unix/epoll.h"
#elif defined(KUN_PLATFORM_DARWIN)
#include "unix/kqueue.h"
#elif defined(KUN_PLATFORM_WIN32)
#include "win/select.h"
#endif

#endif
