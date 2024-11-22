#ifndef KUN_WEB_ABORT_SIGNAL_H
#define KUN_WEB_ABORT_SIGNAL_H

#include <stdint.h>

#include <list>
#include <vector>

#include "v8.h"
#include "env/environment.h"
#include "util/constants.h"
#include "web/event_target.h"

namespace kun::web {

class AbortSignal : public EventTarget {
public:
    AbortSignal(Environment* env, v8::Local<v8::Object> obj) : EventTarget(env, obj, this) {}

    ~AbortSignal() = default;

    bool isAborted() const {
        return !abortReason.IsEmpty();
    }

    void throwIfAborted();

    void onabort(v8::Local<v8::Value> value);

    void addAlgorithm(v8::Local<v8::Function> func);

    void signalAbort(v8::Local<v8::Value> value);

    static v8::Local<v8::Object> abort(Environment* env, v8::Local<v8::Value> value);

    static v8::Local<v8::Object> timeout(Environment* env, int64_t milliseconds);

    static v8::Local<v8::Object> any(
        Environment* env,
        const std::vector<v8::Local<v8::Object>>& signals
    );

    v8::Global<v8::Value> abortReason;
    EventListener* onabortListener{nullptr};
    std::list<v8::Global<v8::Function>> abortAlgorithms;
    std::list<v8::Global<v8::Object>> sourceSignals;
    std::list<v8::Global<v8::Object>> dependentSignals;
    bool dependent{false};
};

void exposeAbortSignal(v8::Local<v8::Context> context, ExposedScope exposedScope);

}

#endif
