#ifndef KUN_UTIL_V8_UTIL_H
#define KUN_UTIL_V8_UTIL_H

#include "v8.h"
#include "util/bstring.h"

namespace kun::util {

BString toBString(
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> value
);

BString formatSourceLine(
    v8::Local<v8::Context> context,
    v8::Local<v8::Message> message
);

BString formatStackTrace(
    v8::Local<v8::Context> context,
    v8::Local<v8::StackTrace> stackTrace
);

BString formatException(
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> exception
);

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

inline void setToStringTag(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate> objTmpl,
    v8::Local<v8::Value> value
) {
    objTmpl->Set(
        v8::Symbol::GetToStringTag(isolate),
        value,
        static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontEnum)
    );
}

inline void setFunction(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate> objTmpl,
    const BString& funcName,
    v8::FunctionCallback funcCallback,
    v8::PropertyAttribute propAttr = v8::None
) {
    objTmpl->Set(
        isolate,
        funcName.c_str(),
        v8::FunctionTemplate::New(
            isolate,
            funcCallback,
            v8::Local<v8::Value>(),
            v8::Local<v8::Signature>(),
            0,
            v8::ConstructorBehavior::kThrow
        ),
        propAttr
    );
}

inline void setFunction(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> obj,
    const BString& funcName,
    v8::FunctionCallback funcCallback,
    v8::PropertyAttribute propAttr = v8::None
) {
    obj->DefineOwnProperty(
        context,
        toV8String(context->GetIsolate(), funcName),
        v8::Function::New(
            context,
            funcCallback,
            v8::Local<v8::Value>(),
            0,
            v8::ConstructorBehavior::kThrow
        ).ToLocalChecked(),
        propAttr
    ).Check();
}

inline void throwError(
    v8::Isolate* isolate,
    const BString& str
) {
    isolate->ThrowException(
        v8::Exception::Error(toV8String(isolate, str))
    );
}

inline void throwRangeError(
    v8::Isolate* isolate,
    const BString& str
) {
    isolate->ThrowException(
        v8::Exception::RangeError(toV8String(isolate, str))
    );
}

inline void throwReferenceError(
    v8::Isolate* isolate,
    const BString& str
) {
    isolate->ThrowException(
        v8::Exception::ReferenceError(toV8String(isolate, str))
    );
}

inline void throwSyntaxError(
    v8::Isolate* isolate,
    const BString& str
) {
    isolate->ThrowException(
        v8::Exception::SyntaxError(toV8String(isolate, str))
    );
}

inline void throwTypeError(
    v8::Isolate* isolate,
    const BString& str
) {
    isolate->ThrowException(
        v8::Exception::TypeError(toV8String(isolate, str))
    );
}

}

#endif
