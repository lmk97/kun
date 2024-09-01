#ifndef KUN_ENV_ENVIRONMENT_H
#define KUN_ENV_ENVIRONMENT_H

#include <vector>

#include "v8.h"
#include "env/cmdline.h"
#include "util/constants.h"

namespace kun {

class EventLoop;
class EsModule;

class Environment {
public:
    Environment(const Environment&) = delete;

    Environment& operator=(const Environment&) = delete;

    Environment(Environment&&) = delete;

    Environment& operator=(Environment&&) = delete;

    Environment(Cmdline* cmdline);

    ~Environment() = default;

    void run(ExposedScope exposedScope);

    void runMicrotask();

    Cmdline* getCmdline() const {
        return cmdline;
    }

    EventLoop* getEventLoop() const {
        return eventLoop;
    }

    EsModule* getEsModule() const {
        return esModule;
    }

    v8::Isolate* getIsolate() const {
        return isolate;
    }

    v8::Local<v8::Context> getContext() const {
        return context.Get(isolate);
    }

    void pushRejection(
        v8::Local<v8::Promise> promise,
        v8::Local<v8::Value> value
    ) {
        unhandledRejections.emplace_back(isolate, promise);
        unhandledRejections.emplace_back(isolate, value);
    }

    void popRejection() {
        if (!unhandledRejections.empty()) {
            unhandledRejections.pop_back();
            unhandledRejections.pop_back();
        }
    }

    BString getKunDir() const {
        return BString::view(kunDir);
    }

    BString getDepsDir() const {
        return BString::view(depsDir);
    }

    static Environment* from(v8::Local<v8::Context> context) {
        return static_cast<Environment*>(
            context->GetAlignedPointerFromEmbedderData(1)
        );
    }

private:
    Cmdline* cmdline;
    EventLoop* eventLoop;
    EsModule* esModule;
    v8::Isolate* isolate;
    v8::Global<v8::Context> context;
    std::vector<v8::Global<v8::Value>> unhandledRejections;
    BString kunDir;
    BString depsDir;
};

}

#endif
