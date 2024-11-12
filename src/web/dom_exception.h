#ifndef KUN_WEB_DOM_EXCEPTION_H
#define KUN_WEB_DOM_EXCEPTION_H

#include "v8.h"
#include "util/constants.h"

namespace kun::web {

void exposeDOMException(v8::Local<v8::Context> context, ExposedScope exposedScope);

}

#endif
