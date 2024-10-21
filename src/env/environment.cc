#include "env/environment.h"

#include "libplatform/libplatform.h"
#include "env/cmdline.h"
#include "loop/event_loop.h"
#include "module/es_module.h"
#include "sys/fs.h"
#include "sys/io.h"
#include "sys/path.h"
#include "sys/process.h"
#include "util/scope_guard.h"
#include "util/v8_utils.h"
#include "web/console.h"

KUN_V8_USINGS;

using v8::PromiseRejectMessage;
using v8::StackTrace;
using kun::BString;
using kun::Environment;
using kun::EsModule;
using kun::EventLoop;
using kun::sys::eprintln;
using kun::sys::getAppDir;
using kun::sys::joinPath;
using kun::sys::makeDirs;
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
        env->pushUnhandledRejection(promise, value);
    } else if (event == v8::kPromiseHandlerAddedAfterReject) {
        env->popUnhandledRejection();
    } else {
        auto value = rejectMessage.GetValue();
        auto errStr = toBString(context, value);
        eprintln("\033[0;31mUncaught (in promise)\033[0m: {}", errStr);
    }
}

}

namespace kun {

Environment::Environment(Cmdline* cmdline): cmdline(cmdline) {
    unhandledRejections.reserve(256);
    auto appDir = getAppDir().unwrap();
    kunDir = joinPath(appDir, ".kun");
    depsDir = joinPath(kunDir, "deps");
    makeDirs(kunDir).expect("makeDirs '{}'", kunDir);
    makeDirs(depsDir).expect("makeDirs '{}'", depsDir);
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
        isolate->SetMicrotasksPolicy(v8::MicrotasksPolicy::kExplicit);
        isolate->SetCaptureStackTraceForUncaughtExceptions(true, 16, StackTrace::kDetailed);
        isolate->SetPromiseRejectCallback(promiseRejectCallback);
        isolate->SetHostImportModuleDynamicallyCallback(esm::importModuleDynamicallyCallback);
        isolate->SetHostInitializeImportMetaObjectCallback(esm::importMetaObjectCallback);
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
            web::exposeConsole(context, exposedScope);
            EsModule esModule(this);
            EventLoop eventLoop(this);
            this->esModule = &esModule;
            this->eventLoop = &eventLoop;
            auto scriptPath = cmdline->getScriptPath();
            if (!scriptPath.empty()) {
                if (esModule.execute(scriptPath)) {
                    eventLoop.run();
                }
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
