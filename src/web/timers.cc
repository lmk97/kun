#include "web/timers.h"

#include "util/js_utils.h"
#include "util/scope_guard.h"
#include "util/utils.h"
#include "util/v8_utils.h"

KUN_V8_USINGS;

using kun::Environment;
using kun::JS;
using kun::WebTimer;
using kun::util::checkFuncArgs;
using kun::util::fromObject;
using kun::util::setFunction;
using kun::util::throwTypeError;

namespace {

void createTimer(const FunctionCallbackInfo<Value>& info, bool repeat) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    if (
        !checkFuncArgs<
        JS::Any,
        JS::Optional | JS::Any
        >(info)
    ) {
        return;
    }
    auto context = isolate->GetCurrentContext();
    auto env = Environment::from(context);
    uint64_t microseconds = 1;
    if (info.Length() > 0) {
        Local<Number> num;
        if (info[1]->ToNumber(context).ToLocal(&num)) {
            auto n = static_cast<int64_t>(num->Value());
            if (n > 0) {
                microseconds = n * 1000;
            }
        }
    }
    uint32_t len = 0;
    if (info.Length() > 2) {
        len = info.Length() - 2;
    }
    auto arr = Array::New(isolate, len);
    for (uint32_t i = 0; i < len; i++) {
        arr->Set(context, i, info[i + 2]).Check();
    }
    auto webTimer = new WebTimer(env, info[0], arr, microseconds, repeat);
    auto id = env->addWebTimer(webTimer);
    webTimer->id = id;
    auto eventLoop = env->getEventLoop();
    if (!eventLoop->addChannel(webTimer)) {
        env->removeWebTimer(id);
        delete webTimer;
        KUN_LOG_ERR("Failed to add WebTimer");
    }
    info.GetReturnValue().Set(id);
}

void releaseTimer(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    if (info.Length() < 1) {
        return;
    }
    auto context = isolate->GetCurrentContext();
    Local<Number> num;
    if (!info[0]->ToNumber(context).ToLocal(&num)) {
        return;
    }
    auto id = static_cast<uint32_t>(num->Value());
    auto env = Environment::from(context);
    auto webTimer = env->removeWebTimer(id);
    if (webTimer != nullptr) {
        auto eventLoop = env->getEventLoop();
        eventLoop->removeChannel(webTimer);
        delete webTimer;
    }
}

void setTimeout(const FunctionCallbackInfo<Value>& info) {
    createTimer(info, false);
}

void clearTimeout(const FunctionCallbackInfo<Value>& info) {
    releaseTimer(info);
}

void setInterval(const FunctionCallbackInfo<Value>& info) {
    createTimer(info, true);
}

void clearInterval(const FunctionCallbackInfo<Value>& info) {
    releaseTimer(info);
}

}

namespace kun {

void WebTimer::onReadable() {
    auto isolate = env->getIsolate();
    HandleScope handleScope(isolate);
    auto context = env->getContext();
    auto value = handler.Get(isolate);
    auto arr = args.Get(isolate);
    const auto len = arr->Length();
    if (value->IsFunction()) {
        uint32_t argc = 0;
        Local<Value>* argv = nullptr;
        ON_SCOPE_EXIT {
            if (argv != nullptr) {
                delete[] argv;
            }
        };
        if (len > 0) {
            argc = len;
            argv = new Local<Value>[argc];
            for (uint32_t i = 0; i < argc; i++) {
                if (!fromObject(context, arr, i, argv[i])) {
                    KUN_LOG_ERR("parameter {} is invalid", i + 3);
                }
            }
        }
        auto func = value.As<Function>();
        auto recv = v8::Undefined(isolate);
        Local<Value> result;
        if (func->Call(context, recv, argc, argv).ToLocal(&result)) {
            env->runMicrotask();
        } else {
            KUN_LOG_ERR("WebTimer callback failed");
        }
    } else {
        auto globalThis = context->Global();
        Local<Function> eval;
        if (fromObject(context, globalThis, "eval", eval)) {
            Local<String> v8Str;
            if (value->ToString(context).ToLocal(&v8Str)) {
                auto recv = v8::Undefined(isolate);
                Local<Value> argv[] = {v8Str};
                Local<Value> result;
                if (eval->Call(context, recv, 1, argv).ToLocal(&result)) {
                    env->runMicrotask();
                }
            } else {
                throwTypeError(isolate, "parameter 1 is not a string");
            }
        } else {
            KUN_LOG_ERR("globalThis.eval is not found");
        }
    }
    if (!repeat) {
        env->removeWebTimer(this->id);
        delete this;
    }
}

namespace web {

void exposeTimers(Local<Context> context, ExposedScope exposedScope) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    auto globalThis = context->Global();
    setFunction(context, globalThis, "setTimeout", setTimeout);
    setFunction(context, globalThis, "clearTimeout", clearTimeout);
    setFunction(context, globalThis, "setInterval", setInterval);
    setFunction(context, globalThis, "clearInterval", clearInterval);
}

}

}
