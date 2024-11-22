#include "web/abort_controller.h"

#include "env/environment.h"
#include "util/internal_field.h"
#include "util/v8_utils.h"
#include "web/abort_signal.h"

KUN_V8_USINGS;

using kun::Environment;
using kun::InternalField;
using kun::web::AbortSignal;
using kun::util::createObject;
using kun::util::defineAccessor;
using kun::util::fromInternal;
using kun::util::getPrototypeOf;
using kun::util::instanceOf;
using kun::util::setFunction;
using kun::util::setToStringTag;
using kun::util::throwTypeError;
using kun::util::toV8String;

namespace {

void newAbortController(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    if (!info.IsConstructCall()) {
        throwTypeError(isolate, "Please use the 'new' operator");
        return;
    }
    auto context = isolate->GetCurrentContext();
    auto env = Environment::from(context);
    auto signal = createObject(context, "AbortSignal", 1).ToLocalChecked();
    new AbortSignal(env, signal);
    auto recv = info.This();
    recv->SetInternalField(0, signal);
}

void getSignal(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    Local<Object> signal;
    if (fromInternal(recv, 0, signal)) {
        info.GetReturnValue().Set(signal);
    }
}

void abort(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    Local<Object> signal;
    if (!fromInternal(recv, 0, signal)) {
        return;
    }
    auto abortSignal = InternalField<AbortSignal>::get(signal, 0);
    if (abortSignal == nullptr) {
        return;
    }
    Local<Value> reason;
    if (info.Length() > 0) {
        reason = info[0];
    }
    abortSignal->signalAbort(reason);
}

}

namespace kun::web {

void exposeAbortController(Local<Context> context, ExposedScope exposedScope) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    auto funcTmpl = FunctionTemplate::New(isolate);
    auto protoTmpl = funcTmpl->PrototypeTemplate();
    auto instTmpl = funcTmpl->InstanceTemplate();
    instTmpl->SetInternalFieldCount(1);
    auto exposedName = toV8String(isolate, "AbortController");
    funcTmpl->SetClassName(exposedName);
    funcTmpl->SetCallHandler(newAbortController);
    setToStringTag(isolate, protoTmpl, exposedName);
    setFunction(isolate, protoTmpl, "abort", abort);
    auto func = funcTmpl->GetFunction(context).ToLocalChecked();
    auto proto = getPrototypeOf(context, func).ToLocalChecked();
    defineAccessor(context, proto, "signal", {getSignal});
    auto globalThis = context->Global();
    globalThis->DefineOwnProperty(context, exposedName, func, v8::DontEnum).Check();
}

}
