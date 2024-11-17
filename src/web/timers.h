#ifndef KUN_WEB_TIMERS_H
#define KUN_WEB_TIMERS_H

#include <stdint.h>

#include <vector>

#include "v8.h"
#include "env/environment.h"
#include "loop/timer.h"
#include "util/constants.h"

namespace kun {

class WebTimer : public Timer {
public:
    WebTimer(
        Environment* env,
        v8::Local<v8::Value> handler,
        std::vector<v8::Global<v8::Value>>&& args,
        uint64_t microseconds,
        bool repeat
    ) :
        Timer(microseconds, TimeUnit::MICROSECOND, repeat),
        env(env),
        handler(env->getIsolate(), handler),
        args(std::move(args))
    {

    }

    ~WebTimer() = default;

    void onReadable() override final;

    uint32_t id{0};

private:
    Environment* env;
    v8::Global<v8::Value> handler;
    std::vector<v8::Global<v8::Value>> args;
};

namespace web {

void exposeTimers(v8::Local<v8::Context> context, ExposedScope exposedScope);

}

}

#endif
