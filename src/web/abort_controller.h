#ifndef KUN_WEB_ABORT_CONTROLLER_H
#define KUN_WEB_ABORT_CONTROLLER_H

#include "v8.h"
#include "util/constants.h"

namespace kun::web {

void exposeAbortController(v8::Local<v8::Context> context, ExposedScope exposedScope);

}

#endif
