#ifndef KUN_UNIX_FS_H
#define KUN_UNIX_FS_H

#include "util/constants.h"

#ifdef KUN_PLATFORM_UNIX

#include "util/bstring.h"
#include "util/result.h"

namespace KUN_SYS {

Result<BString> readFile(const BString& path);

Result<bool> makeDirs(const BString& path);

Result<bool> removeDir(const BString& path);

}

#endif

#endif
