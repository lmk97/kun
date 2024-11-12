#ifndef KUN_WEB_EVENT_TARGET_H
#define KUN_WEB_EVENT_TARGET_H

#include <map>

#include "v8.h"
#include "env/environment.h"
#include "util/bstring.h"
#include "util/constants.h"
#include "util/internal_field.h"
#include "util/weak_object.h"
#include "web/event.h"

namespace kun::web {

class EventListener {
public:
    EventListener(const EventListener&) = delete;

    EventListener& operator=(const EventListener&) = delete;

    EventListener(EventListener&&) = default;

    EventListener& operator=(EventListener&&) = default;

    EventListener() = default;

    ~EventListener() = default;

    v8::Global<v8::Object> callback;
    v8::Global<v8::Object> signal;
    bool passive;
    bool once{false};
    bool capture{false};
    bool removed{false};
};

class EventTarget {
public:
    EventTarget(Environment* env, v8::Local<v8::Object> obj, EventTarget* eventTarget) :
        env(env),
        weakObject(obj, eventTarget),
        internalField(eventTarget)
    {
        internalField.set(obj, 0);
    }

    EventTarget(Environment* env, v8::Local<v8::Object> obj) : EventTarget(env, obj, this) {}

    virtual ~EventTarget() = default;

    virtual EventListener* addEventListener(const BString& type, EventListener&& listener);

    virtual void removeEventListener(const BString& type, const EventListener& listener);

    virtual bool dispatchEvent(Event* event);

    Environment* env;
    WeakObject<EventTarget> weakObject;
    InternalField<EventTarget> internalField;
    std::multimap<BString, EventListener> listeners;
};

void exposeEventTarget(v8::Local<v8::Context> context, ExposedScope exposedScope);

}

#endif
