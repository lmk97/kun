#include "web/abort_signal.h"

#include "util/js_utils.h"
#include "util/utils.h"
#include "util/v8_utils.h"

KUN_V8_USINGS;

using kun::Environment;
using kun::InternalField;
using kun::JS;
using kun::web::AbortSignal;
using kun::util::checkFuncArgs;
using kun::util::createObject;
using kun::util::defineAccessor;
using kun::util::fromObject;
using kun::util::getPrototypeOf;
using kun::util::inherit;
using kun::util::instanceOf;
using kun::util::newInstance;
using kun::util::setFunction;
using kun::util::setToStringTag;
using kun::util::throwTypeError;
using kun::util::toV8String;

namespace {

void newAbortSignal(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    throwTypeError(isolate, "Illegal constructor");
}

void throwIfAborted(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto abortSignal = InternalField<AbortSignal>::get(recv, 0);
    if (abortSignal == nullptr) {
        return;
    }
    abortSignal->throwIfAborted();
}

void getAborted(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto abortSignal = InternalField<AbortSignal>::get(recv, 0);
    if (abortSignal == nullptr) {
        return;
    }
    info.GetReturnValue().Set(abortSignal->isAborted());
}

void getReason(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto abortSignal = InternalField<AbortSignal>::get(recv, 0);
    if (abortSignal == nullptr) {
        return;
    }
    if (!abortSignal->abortReason.IsEmpty()) {
        auto reason = abortSignal->abortReason.Get(isolate);
        info.GetReturnValue().Set(reason);
    }
}

void getOnabort(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto abortSignal = InternalField<AbortSignal>::get(recv, 0);
    if (abortSignal == nullptr) {
        return;
    }
    auto onabortListener = abortSignal->onabortListener;
    if (onabortListener != nullptr && !onabortListener->removed) {
        auto onabort = onabortListener->callback.Get(isolate);
        info.GetReturnValue().Set(onabort);
    } else {
        info.GetReturnValue().SetNull();
    }
}

void setOnabort(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    if (!checkFuncArgs<JS::Any>(info)) {
        return;
    }
    auto recv = info.This();
    auto abortSignal = InternalField<AbortSignal>::get(recv, 0);
    if (abortSignal == nullptr) {
        return;
    }
    abortSignal->onabort(info[0]);
}

void abort(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto context = isolate->GetCurrentContext();
    auto env = Environment::from(context);
    Local<Value> reason;
    if (info.Length() > 0) {
        reason = info[0];
    }
    auto signalObj = AbortSignal::abort(env, reason);
    info.GetReturnValue().Set(signalObj);
}

void timeout(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    if (!checkFuncArgs<JS::Any>(info)) {
        return;
    }
    auto context = isolate->GetCurrentContext();
    int64_t milliseconds = -1;
    Local<Number> num;
    if (info[0]->ToNumber(context).ToLocal(&num)) {
        milliseconds = static_cast<int64_t>(num->Value());
    }
    if (milliseconds < 0) {
        throwTypeError(isolate, "requires a non-negative number");
        return;
    }
    auto env = Environment::from(context);
    auto signalObj = AbortSignal::timeout(env, milliseconds);
    info.GetReturnValue().Set(signalObj);
}

void any(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    if (!checkFuncArgs<JS::Object>(info)) {
        return;
    }
    auto context = isolate->GetCurrentContext();
    auto obj = info[0].As<Object>();
    Local<Function> func;
    if (!fromObject(context, obj, v8::Symbol::GetIterator(isolate), func)) {
        throwTypeError(isolate, "parameter 1 is not iterable");
        return;
    }
    Local<Object> iterator;
    if (
        Local<Value> value;
        func->Call(context, obj, 0, nullptr).ToLocal(&value) &&
        !value->IsNull() &&
        value->IsObject()
    ) {
        iterator = value.As<Object>();
    } else {
        throwTypeError(isolate, "parameter 1 is not iterable");
        return;
    }
    v8::Local<v8::Function> next;
    if (!fromObject(context, iterator, "next", next)) {
        throwTypeError(isolate, "parameter 1 is not iterable");
        return;
    }
    std::vector<Local<Value>> signals;
    signals.reserve(128);
    bool done = false;
    while (!done) {
        Local<Object> iteratorResult;
        if (
            Local<Value> value;
            next->Call(context, iterator, 0, nullptr).ToLocal(&value) &&
            !value->IsNull() &&
            value->IsObject()
        ) {
            iteratorResult = value.As<Object>();
        } else {
            throwTypeError(isolate, "parameter 1 is not iterable");
            return;
        }
        if (fromObject(context, iteratorResult, "done", done) && done) {
            break;
        }
        Local<Value> signal;
        if (!fromObject(context, iteratorResult, "value", signal)) {
            throwTypeError(isolate, "Invalid JavaScript value");
            return;
        }
        signals.emplace_back(signal);
    }
    auto env = Environment::from(context);
    auto signalObj = AbortSignal::any(env, signals);
    info.GetReturnValue().Set(signalObj);
}

}

namespace kun::web {

void AbortSignal::throwIfAborted() {
    if (isAborted()) {
        auto isolate = env->getIsolate();
        HandleScope handleScope(isolate);
        auto reason = abortReason.Get(isolate);
        isolate->ThrowException(reason);
    }
}

void AbortSignal::onabort(Local<Value> value) {
    auto isolate = env->getIsolate();
    HandleScope handleScope(isolate);
    if (!value.IsEmpty() && value->IsFunction()) {
        auto func = value.As<Function>();
        if (onabortListener == nullptr) {
            EventListener listener;
            listener.callback.Reset(isolate, func);
            onabortListener = addEventListener("abort", std::move(listener));
        } else {
            onabortListener->callback.Reset(isolate, func);
        }
    } else {
        if (onabortListener != nullptr) {
            onabortListener->removed = true;
            onabortListener->callback.Reset();
            onabortListener->signal.Reset();
        }
    }
}

void AbortSignal::addAlgorithm(Local<Function> func) {
    if (isAborted()) {
        return;
    }
    auto isolate = env->getIsolate();
    abortAlgorithms.emplace_back(isolate, func);
}

void AbortSignal::signalAbort(Local<Value> reason) {
    if (isAborted()) {
        return;
    }
    auto isolate = env->getIsolate();
    HandleScope handleScope(isolate);
    auto context = env->getContext();
    if (reason.IsEmpty()) {
        reason = newInstance(
            context,
            "DOMException"
            "The signal has been aborted",
            "AbortError"
        ).ToLocalChecked();
    }
    abortReason.Reset(isolate, reason);
    auto recv = v8::Undefined(isolate);
    for (const auto& algorithm : abortAlgorithms) {
        auto func = algorithm.Get(isolate);
        Local<Value> result;
        if (!func->Call(context, recv, 0, nullptr).ToLocal(&result)) {
            KUN_LOG_ERR("Failed to invoke algorithm");
        }
    }
    abortAlgorithms.clear();
    auto eventObj = newInstance(context, "Event", "abort").ToLocalChecked();
    auto event = InternalField<Event>::get(eventObj, 0);
    event->isTrusted = true;
    dispatchEvent(event);
    for (const auto& dependentSignal : dependentSignals) {
        auto signalObj = dependentSignal.Get(isolate);
        auto signal = InternalField<AbortSignal>::get(signalObj, 0);
        if (signal == nullptr) {
            signal->signalAbort(reason);
        }
    }
    dependentSignals.clear();
    sourceSignals.clear();
}

Local<Object> AbortSignal::abort(Environment* env, Local<Value> reason) {
    auto isolate = env->getIsolate();
    EscapableHandleScope handleScope(isolate);
    auto context = env->getContext();
    auto signalObj = createObject(context, "AbortSignal", 1).ToLocalChecked();
    if (signalObj.IsEmpty()) {
        return signalObj;
    }
    auto abortSignal = new AbortSignal(env, signalObj);
    if (!reason.IsEmpty() && !reason->IsUndefined()) {
        abortSignal->abortReason.Reset(isolate, reason);
    } else {
        auto e = newInstance(
            context,
            "DOMException",
            "signal is aborted without reason",
            "AbortError"
        ).ToLocalChecked();
        abortSignal->abortReason.Reset(isolate, e);
    }
    return handleScope.Escape(signalObj);
}

Local<Object> AbortSignal::timeout(Environment* env, uint64_t milliseconds) {
    return Local<Object>();
}

Local<Object> AbortSignal::any(Environment* env, const std::vector<Local<Value>>& signals) {
    return Local<Object>();
}

void exposeAbortSignal(Local<Context> context, ExposedScope exposedScope) {

}

}
