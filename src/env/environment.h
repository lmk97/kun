#ifndef KUN_ENV_ENVIRONMENT_H
#define KUN_ENV_ENVIRONMENT_H

#include <stdint.h>

#include <vector>
#include <unordered_map>

#include "v8.h"
#include "util/bstring.h"
#include "util/constants.h"

namespace kun {

class Cmdline;
class EsModule;
class EventLoop;
class WebTimer;

class Environment {
public:
    Environment(const Environment&) = delete;

    Environment& operator=(const Environment&) = delete;

    Environment(Environment&&) = delete;

    Environment& operator=(Environment&&) = delete;

    explicit Environment(Cmdline* cmdline);

    ~Environment() = default;

    void run(ExposedScope exposedScope);

    void runMicrotask();

    Cmdline* getCmdline() const {
        return cmdline;
    }

    EsModule* getEsModule() const {
        return esModule;
    }

    EventLoop* getEventLoop() const {
        return eventLoop;
    }

    v8::Isolate* getIsolate() const {
        return isolate;
    }

    v8::Local<v8::Context> getContext() const {
        return context.Get(isolate);
    }

    void pushUnhandledRejection(v8::Local<v8::Promise> promise, v8::Local<v8::Value> value) {
        unhandledRejections.emplace_back(isolate, promise);
        unhandledRejections.emplace_back(isolate, value);
    }

    void popUnhandledRejection() {
        if (!unhandledRejections.empty()) {
            unhandledRejections.pop_back();
            unhandledRejections.pop_back();
        }
    }

    uint32_t addWebTimer(WebTimer* webTimer) {
        uint32_t id;
        while (true) {
            if (webTimerId == 0) {
                ++webTimerId;
            }
            id = webTimerId++;
            auto pair = webTimerMap.emplace(id, webTimer);
            if (pair.second) {
                break;
            }
        }
        return id;
    }

    WebTimer* removeWebTimer(uint32_t id) {
        auto iter = webTimerMap.find(id);
        if (iter != webTimerMap.end()) {
            auto webTimer = iter->second;
            webTimerMap.erase(iter);
            return webTimer;
        }
        return nullptr;
    }

    BString getKunDir() const {
        return BString::view(kunDir);
    }

    BString getDepsDir() const {
        return BString::view(depsDir);
    }

    static Environment* from(v8::Local<v8::Context> context) {
        return static_cast<Environment*>(context->GetAlignedPointerFromEmbedderData(1));
    }

private:
    Cmdline* cmdline;
    EsModule* esModule;
    EventLoop* eventLoop;
    v8::Isolate* isolate;
    v8::Global<v8::Context> context;
    std::vector<v8::Global<v8::Value>> unhandledRejections;
    std::unordered_map<uint32_t, WebTimer*> webTimerMap;
    uint32_t webTimerId{1};
    BString kunDir;
    BString depsDir;
};

}

#endif
