#include "web/text_encoder.h"

#include "util/js_utils.h"
#include "util/v8_utils.h"

KUN_V8_USINGS;

using v8::Name;
using kun::JS;
using kun::util::checkFuncArgs;
using kun::util::defineAccessor;
using kun::util::getPrototypeOf;
using kun::util::setFunction;
using kun::util::setToStringTag;
using kun::util::throwTypeError;
using kun::util::toV8String;

namespace {

void newTextEncoder(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    if (!info.IsConstructCall()) {
        throwTypeError(isolate, "Please use the 'new' operator");
    }
}

void encode(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    if (!checkFuncArgs<JS::Optional | JS::Any>(info)) {
        return;
    }
    auto context = isolate->GetCurrentContext();
    Local<String> input;
    int inputLen = 0;
    if (info.Length() > 0) {
        if (info[0]->ToString(context).ToLocal(&input)) {
            inputLen = input->Utf8Length(isolate);
        } else {
            throwTypeError(isolate, "Failed to convert value to 'string'");
            return;
        }
    }
    if (input.IsEmpty() || inputLen <= 0) {
        auto arrBuf = ArrayBuffer::New(isolate, 0);
        auto u8Arr = Uint8Array::New(arrBuf, 0, 0);
        info.GetReturnValue().Set(u8Arr);
        return;
    }
    auto buf = new char[inputLen];
    input->WriteUtf8(isolate, buf, inputLen);
    auto store = ArrayBuffer::NewBackingStore(
        buf,
        inputLen,
        [](void* data, size_t, void*) {
            auto buf = static_cast<char*>(data);
            delete[] buf;
        },
        nullptr
    );
    auto arrBuf = ArrayBuffer::New(isolate, std::move(store));
    auto u8Arr = Uint8Array::New(arrBuf, 0, inputLen);
    isolate->AdjustAmountOfExternalAllocatedMemory(inputLen);
    info.GetReturnValue().Set(u8Arr);
}

void encodeInto(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    if (!checkFuncArgs<JS::Any, JS::Uint8Array>(info)) {
        return;
    }
    auto context = isolate->GetCurrentContext();
    Local<String> source;
    if (!info[0]->ToString(context).ToLocal(&source)) {
        throwTypeError(isolate, "Failed to convert value to 'string'");
        return;
    }
    auto destination = info[1].As<Uint8Array>();
    const auto nbytes = static_cast<int>(destination->ByteLength());
    const auto len = source->Utf8Length(isolate);
    const auto minLen = nbytes >= len ? len : nbytes;
    int read = 0;
    int written = 0;
    if (minLen > 0) {
        auto arrBuf = destination->Buffer();
        auto offset = destination->ByteOffset();
        auto data = static_cast<char*>(arrBuf->Data()) + offset;
        written = source->WriteUtf8(isolate, data, minLen, &read);
    }
    Local<Name> names[] = {
        toV8String(isolate, "read"),
        toV8String(isolate, "written")
    };
    Local<Value> values[] = {
        Number::New(isolate, read),
        Number::New(isolate, written)
    };
    auto obj = Object::New(isolate, v8::Null(isolate), names, values, 2);
    info.GetReturnValue().Set(obj);
}

void getEncoding(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto encoding = toV8String(isolate, "utf-8");
    info.GetReturnValue().Set(encoding);
}

}

namespace kun::web {

void exposeTextEncoder(Local<Context> context, ExposedScope exposedScope) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    auto funcTmpl = FunctionTemplate::New(isolate);
    auto protoTmpl = funcTmpl->PrototypeTemplate();
    auto exposedName = toV8String(isolate, "TextEncoder");
    funcTmpl->SetClassName(exposedName);
    funcTmpl->SetCallHandler(newTextEncoder);
    setToStringTag(isolate, protoTmpl, exposedName);
    setFunction(isolate, protoTmpl, "encode", encode);
    setFunction(isolate, protoTmpl, "encodeInto", encodeInto);
    auto func = funcTmpl->GetFunction(context).ToLocalChecked();
    defineAccessor(context, func, "encoding", {getEncoding});
    auto globalThis = context->Global();
    globalThis->DefineOwnProperty(context, exposedName, func, v8::DontEnum).Check();
}

}
