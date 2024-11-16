#include "web/abort_signal.h"

#include "util/js_utils.h"
#include "util/utils.h"
#include "util/v8_utils.h"

KUN_V8_USINGS;

using v8::External;
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

void timeoutCallback(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto context = isolate->GetCurrentContext();
    auto external = info.Data().As<External>();
    auto abortSignal = static_cast<AbortSignal*>(external->Value());
    auto e = newInstance(
        context,
        "DOMException",
        "signal timed out",
        "TimeoutError"
    ).ToLocalChecked();
    abortSignal->signalAbort(e);
}

void appendIfAbsent(std::list<Global<Object>>& list, Isolate* isolate, Local<Object> obj) {
    auto target = Global<Object>(isolate, obj);
    for (const auto& g : list) {
        if (g == target) {
            return;
        }
    }
    list.emplace_back(std::move(target));
}

void newAbortSignal(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    throwTypeError(isolate, "Illegal constructor");
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
    auto signal = AbortSignal::abort(env, reason);
    info.GetReturnValue().Set(signal);
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
    auto env = Environment::from(context);
    auto signal = AbortSignal::timeout(env, milliseconds);
    info.GetReturnValue().Set(signal);
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
    std::vector<Local<Object>> signals;
    signals.reserve(64);
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
        if (
            Local<Object> signal;
            fromObject(context, iteratorResult, "value", signal)
        ) {
            signals.emplace_back(signal);
        } else {
            throwTypeError(isolate, "Failed to convert value to 'object'");
            return;
        }
    }
    auto env = Environment::from(context);
    auto signal = AbortSignal::any(env, signals);
    info.GetReturnValue().Set(signal);
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
        }
    }
}

void AbortSignal::addAlgorithm(Local<Function> func) {
    if (isAborted()) {
        return;
    }
    auto isolate = env->getIsolate();
    HandleScope handleScope(isolate);
    abortAlgorithms.emplace_back(isolate, func);
}

void AbortSignal::runAbortSteps() {
    auto isolate = env->getIsolate();
    HandleScope handleScope(isolate);
    auto context = env->getContext();
    auto recv = v8::Undefined(isolate);
    for (const auto& algorithm : abortAlgorithms) {
        auto func = algorithm.Get(isolate);
        Local<Value> result;
        if (!func->Call(context, recv, 0, nullptr).ToLocal(&result)) {
            KUN_LOG_ERR("Failed to invoke 'algorithm'");
        }
    }
    abortAlgorithms.clear();
    auto eventObj = newInstance(context, "Event", "abort").ToLocalChecked();
    auto event = InternalField<Event>::get(eventObj, 0);
    event->isTrusted = true;
    dispatchEvent(event);
}

void AbortSignal::signalAbort(Local<Value> value) {
    if (isAborted()) {
        return;
    }
    auto isolate = env->getIsolate();
    HandleScope handleScope(isolate);
    auto context = env->getContext();
    if (value.IsEmpty()) {
        value = newInstance(
            context,
            "DOMException"
            "The signal has been aborted",
            "AbortError"
        ).ToLocalChecked();
    }
    abortReason.Reset(isolate, value);
    std::list<AbortSignal*> dependentSignalsToAbort;
    for (const auto& g : dependentSignals) {
        auto signal = g.Get(isolate);
        auto abortSignal = InternalField<AbortSignal>::get(signal, 0);
        if (!abortSignal->isAborted()) {
            abortSignal->abortReason.Reset(isolate, value);
            dependentSignalsToAbort.emplace_back(abortSignal);
        }
    }
    runAbortSteps();
    for (const auto& abortSignal : dependentSignalsToAbort) {
        abortSignal->runAbortSteps();
    }
    sourceSignals.clear();
    dependentSignals.clear();
}

Local<Object> AbortSignal::abort(Environment* env, Local<Value> value) {
    auto isolate = env->getIsolate();
    EscapableHandleScope handleScope(isolate);
    auto context = env->getContext();
    auto signal = createObject(context, "AbortSignal", 1).ToLocalChecked();
    auto abortSignal = new AbortSignal(env, signal);
    if (!value.IsEmpty() && !value->IsUndefined()) {
        abortSignal->abortReason.Reset(isolate, value);
    } else {
        auto e = newInstance(
            context,
            "DOMException",
            "signal is aborted without reason",
            "AbortError"
        ).ToLocalChecked();
        abortSignal->abortReason.Reset(isolate, e);
    }
    return handleScope.Escape(signal);
}

Local<Object> AbortSignal::timeout(Environment* env, int64_t milliseconds) {
    auto isolate = env->getIsolate();
    EscapableHandleScope handleScope(isolate);
    auto context = env->getContext();
    auto signal = createObject(context, "AbortSignal", 1).ToLocalChecked();
    auto abortSignal = new AbortSignal(env, signal);
    if (milliseconds < 0) {
        throwTypeError(isolate, "requires a non-negative number");
        return handleScope.Escape(signal);
    }
    auto globalThis = context->Global();
    Local<Function> setTimeout;
    if (!fromObject(context, globalThis, "setTimeout", setTimeout)) {
        KUN_LOG_ERR("globalThis.setTimeout is not found");
        return handleScope.Escape(signal);
    }
    auto recv = v8::Undefined(isolate);
    auto data = External::New(isolate, abortSignal);
    auto callback = Function::New(
        context,
        timeoutCallback,
        data
    ).ToLocalChecked();
    Local<Value> argv[] = {
        callback,
        Number::New(isolate, milliseconds)
    };
    Local<Value> result;
    if (!setTimeout->Call(context, recv, 2, argv).ToLocal(&result)) {
        KUN_LOG_ERR("Failed to invoke 'setTimeout'");
    }
    return handleScope.Escape(signal);
}

Local<Object> AbortSignal::any(Environment* env, const std::vector<Local<Object>>& signals) {
    auto isolate = env->getIsolate();
    EscapableHandleScope handleScope(isolate);
    auto context = env->getContext();
    auto resultSignal = createObject(context, "AbortSignal", 1).ToLocalChecked();
    auto resultAbortSignal = new AbortSignal(env, resultSignal);
    for (const auto& signal : signals) {
        if (!instanceOf(context, signal, "AbortSignal")) {
            throwTypeError(isolate, "Failed to convert value to 'AbortSignal'");
            continue;
        }
        auto abortSignal = InternalField<AbortSignal>::get(signal, 0);
        if (abortSignal == nullptr) {
            continue;
        }
        if (abortSignal->isAborted()) {
            auto reason = abortSignal->abortReason.Get(isolate);
            resultAbortSignal->abortReason.Reset(isolate, reason);
            return handleScope.Escape(resultSignal);
        }
    }
    resultAbortSignal->dependent = true;
    for (const auto& signal : signals) {
        auto abortSignal = InternalField<AbortSignal>::get(signal, 0);
        if (!abortSignal->dependent) {
            appendIfAbsent(resultAbortSignal->sourceSignals, isolate, signal);
            appendIfAbsent(abortSignal->dependentSignals, isolate, resultSignal);
        } else {
            const auto& sourceSignals = abortSignal->sourceSignals;
            for (const auto& g : sourceSignals) {
                auto sourceSignal = g.Get(isolate);
                auto sourceAbortSignal = InternalField<AbortSignal>::get(sourceSignal, 0);
                if (sourceAbortSignal->isAborted() || sourceAbortSignal->dependent) {
                    throwTypeError(isolate, "source signal is aborted or dependent");
                    break;
                }
                appendIfAbsent(resultAbortSignal->sourceSignals, isolate, sourceSignal);
                appendIfAbsent(sourceAbortSignal->dependentSignals, isolate, resultSignal);
            }
        }
    }
    return handleScope.Escape(resultSignal);
}

void exposeAbortSignal(Local<Context> context, ExposedScope exposedScope) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    auto funcTmpl = FunctionTemplate::New(isolate);
    auto protoTmpl = funcTmpl->PrototypeTemplate();
    auto instTmpl = funcTmpl->InstanceTemplate();
    instTmpl->SetInternalFieldCount(1);
    auto exposedName = toV8String(isolate, "AbortSignal");
    funcTmpl->SetClassName(exposedName);
    funcTmpl->SetCallHandler(newAbortSignal);
    setToStringTag(isolate, protoTmpl, exposedName);
    setFunction(isolate, protoTmpl, "throwIfAborted", throwIfAborted);
    auto func = funcTmpl->GetFunction(context).ToLocalChecked();
    auto proto = getPrototypeOf(context, func).ToLocalChecked();
    defineAccessor(context, proto, "aborted", {getAborted});
    defineAccessor(context, proto, "reason", {getReason});
    defineAccessor(context, proto, "onabort", {getOnabort, setOnabort});
    func->DefineOwnProperty(
        context,
        toV8String(isolate, "abort"),
        Function::New(context, abort).ToLocalChecked()
    ).Check();
    func->DefineOwnProperty(
        context,
        toV8String(isolate, "timeout"),
        Function::New(context, timeout).ToLocalChecked()
    ).Check();
    func->DefineOwnProperty(
        context,
        toV8String(isolate, "any"),
        Function::New(context, any).ToLocalChecked()
    ).Check();
    inherit(context, func, "EventTarget");
    auto globalThis = context->Global();
    globalThis->DefineOwnProperty(context, exposedName, func, v8::DontEnum).Check();
}

}
