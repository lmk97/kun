#include "env/environment.h"

#include "libplatform/libplatform.h"
#include "loop/event_loop.h"
#include "module/es_module.h"
#include "sys/io.h"
#include "sys/path.h"
#include "sys/process.h"
#include "util/scope_guard.h"
#include "util/v8_util.h"

KUN_V8_USINGS;

using v8::PromiseRejectMessage;
using v8::StackTrace;
using kun::Environment;
using kun::EsModule;
using kun::EventLoop;
using kun::sys::eprintln;
using kun::sys::getAppDir;
using kun::sys::joinPath;
using kun::util::formatException;
using kun::util::toBString;
using kun::util::toV8String;

namespace {

void promiseRejectCallback(PromiseRejectMessage rejectMessage) {
    auto promise = rejectMessage.GetPromise();
    auto isolate = promise->GetIsolate();
    HandleScope handleScope(isolate);
    auto context = isolate->GetCurrentContext();
    auto env = Environment::from(context);
    auto event = rejectMessage.GetEvent();
    if (event == v8::kPromiseRejectWithNoHandler) {
        auto value = rejectMessage.GetValue();
        env->pushRejection(promise, value);
    } else if (event == v8::kPromiseHandlerAddedAfterReject) {
        env->popRejection();
    } else {
        auto errStr = toBString(context, rejectMessage.GetValue());
        eprintln("\033[0;31mUncaught (in promise)\033[0m: {}", errStr);
    }
}

}

namespace kun {

Environment::Environment(Cmdline* cmdline): cmdline(cmdline) {
    unhandledRejections.reserve(1024);
    auto appDir = getAppDir().unwrap();
    kunDir = joinPath(appDir, ".kun");
    depsDir = joinPath(kunDir, "deps");
}

void Environment::run(ExposedScope exposedScope) {
    auto platform = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform.get());
    auto v8Flags = cmdline->get<BString>(Cmdline::V8_FLAGS).unwrap();
    if (!v8Flags.empty()) {
        v8::V8::SetFlagsFromString(v8Flags.c_str());
    }
    v8::V8::Initialize();
    auto abAllocator = ArrayBuffer::Allocator::NewDefaultAllocator();
    ON_SCOPE_EXIT {
        delete abAllocator;
    };
    Isolate::CreateParams createParams;
    createParams.array_buffer_allocator = abAllocator;
    auto isolate = Isolate::New(createParams);
    {
        Isolate::Scope isolateScope(isolate);
        HandleScope handleScope(isolate);
        isolate->SetCaptureStackTraceForUncaughtExceptions(
            true,
            64,
            StackTrace::kDetailed
        );
        isolate->SetHostImportModuleDynamicallyCallback(
            esm::importModuleDynamicallyCallback
        );
        isolate->SetHostInitializeImportMetaObjectCallback(
            esm::importMetaObjectCallback
        );
        isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kExplicit);
        isolate->SetPromiseRejectCallback(promiseRejectCallback);
        {
            auto objTmpl = ObjectTemplate::New(isolate);
            auto context = Context::New(isolate, nullptr, objTmpl);
            Context::Scope contextScope(context);
            context->SetAlignedPointerInEmbedderData(1, this);
            auto globalThis = context->Global();
            globalThis->DefineOwnProperty(
                context,
                toV8String(isolate, KUN_NAME),
                Object::New(isolate),
                v8::ReadOnly
            ).Check();
            this->isolate = isolate;
            this->context.Reset(isolate, context);
            EventLoop eventLoop(this);
            EsModule esModule(this);
            this->eventLoop = &eventLoop;
            this->esModule = &esModule;
            auto scriptPath = cmdline->getScriptPath();
            if (esModule.execute(scriptPath)) {
                eventLoop.run();
            }
            this->isolate = nullptr;
            this->context.Reset();
        }
    }
    isolate->ContextDisposedNotification();
    isolate->LowMemoryNotification();
    isolate->ClearKeptObjects();
    isolate->Dispose();
    v8::V8::Dispose();
    v8::V8::DisposePlatform();
}

void Environment::runMicrotask() {
    isolate->PerformMicrotaskCheckpoint();
    if (unhandledRejections.empty()) {
        return;
    }
    HandleScope handleScope(isolate);
    auto context = getContext();
    auto begin = unhandledRejections.cbegin();
    auto end = unhandledRejections.cend();
    for (auto iter = begin; iter != end; ++iter) {
        ++iter;
        auto value = iter->Get(isolate);
        auto errStr = formatException(context, value);
        eprintln(errStr);
    }
    unhandledRejections.clear();
}

}
