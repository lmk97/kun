#ifndef KUN_WEB_TEXT_ENCODER_H
#define KUN_WEB_TEXT_ENCODER_H

#include "v8.h"
#include "util/constants.h"

namespace kun::web {

void exposeTextEncoder(v8::Local<v8::Context> context, ExposedScope exposedScope);

}

#endif
