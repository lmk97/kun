#ifndef KUN_WEB_CONSOLE_H
#define KUN_WEB_CONSOLE_H

#include <stdint.h>

#include <unordered_map>

#include "v8.h"
#include "util/bstring.h"
#include "util/constants.h"
#include "util/internal_field.h"
#include "util/weak_object.h"

namespace kun::web {

class Console {
public:
    Console(v8::Local<v8::Object> obj) : weakObject(obj, this), internalField(this) {
        countMap.reserve(128);
        timerTable.reserve(128);
    }

    static Console* from(v8::Local<v8::Context> context);

    WeakObject<Console> weakObject;
    InternalField<Console> internalField;
    std::unordered_map<BString, uint64_t, BStringHash> countMap;
    std::unordered_map<BString, uint64_t, BStringHash> timerTable;
    uint32_t groupStack{0};
    uint32_t indents{0};
    int32_t depth{2};
    bool showHidden{false};
};

void exposeConsole(v8::Local<v8::Context> context, ExposedScope exposedScope);

}

#endif
