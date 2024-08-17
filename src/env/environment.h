#ifndef KUN_ENV_ENVIRONMENT_H
#define KUN_ENV_ENVIRONMENT_H

#include "v8.h"
#include "env/options.h"

namespace kun {

class EventLoop;

class Environment {
public:
    Environment(const Environment&) = delete;

    Environment& operator=(const Environment&) = delete;

    Environment(Environment&&) = delete;

    Environment& operator=(Environment&&) = delete;

    Environment(Options* options);

    ~Environment() = default;

    Options* getOptions() const {
        return options;
    }

    EventLoop* getEventLoop() const {
        return eventLoop;
    }

    void setEventLoop(EventLoop* eventLoop) {
        if (this->eventLoop == nullptr) {
            this->eventLoop = eventLoop;
        }
    }

    v8::Isolate* getIsolate() const {
        return isolate;
    }

    v8::Local<v8::Context> getContext() const {
        return context.Get(isolate);
    }

private:
    Options* options;
    EventLoop* eventLoop{ nullptr };
    v8::Isolate* isolate;
    v8::Global<v8::Context> context;
};

}

#endif
