#ifndef KUN_UTIL_UTIL_H
#define KUN_UTIL_UTIL_H

#include <stdio.h>
#include <stdint.h>
#include <errno.h>
#include <time.h>

#include <string>
#include <codecvt>

#include "util/result.h"
#include "util/bstring.h"

namespace kun::util {

BString dirname(const BString& path);

BString dirname(BString&& path);

BString normalizePath(const BString& path);

BString joinPath(const BString& dir, const BString& path);

BString toAbsolutePath(const BString& path);

BString toAbsolutePath(BString&& path);

}

#endif
