#ifndef KUN_WEB_TEXT_DECODER_H
#define KUN_WEB_TEXT_DECODER_H

#include "v8.h"
#include "env/environment.h"
#include "util/bstring.h"
#include "util/constants.h"
#include "util/internal_field.h"
#include "util/weak_object.h"

namespace kun::web {

class TextDecoder {
public:
    TextDecoder(Environment* env, v8::Local<v8::Object> obj) :
        env(env),
        weakObject(obj, this),
        internalField(this),
        errorMode("replacement")
    {
        internalField.set(obj, 0);
    }

    Environment* env;
    WeakObject<TextDecoder> weakObject;
    InternalField<TextDecoder> internalField;
    BString encoding;
    BString errorMode;
    BString ioQueue;
    bool ignoreBOM{false};
    bool doNotFlush{false};
};

void exposeTextDecoder(v8::Local<v8::Context> context, ExposedScope exposedScope);

}

#endif
