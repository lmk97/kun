#ifndef KUN_UTIL_V8_UTIL_H
#define KUN_UTIL_V8_UTIL_H

#include "v8.h"
#include "util/bstring.h"

namespace kun::util {

inline v8::Local<v8::String> toV8String(
    v8::Isolate* isolate,
    const BString& str
) {
    return v8::String::NewFromUtf8(
        isolate,
        str.data(),
        v8::NewStringType::kNormal,
        static_cast<int>(str.length())
    ).ToLocalChecked();
}

}

#endif
