#ifndef KUN_WEB_WEB_H
#define KUN_WEB_WEB_H

#include "v8.h"
#include "util/constants.h"

namespace kun::web {

void expose(v8::Local<v8::Context> context, ExposedScope exposedScope);

}

#endif
