#ifndef KUN_WEB_EVENT_H
#define KUN_WEB_EVENT_H

#include <vector>

#include "v8.h"
#include "util/constants.h"
#include "util/internal_field.h"
#include "util/weak_object.h"

namespace kun::web {

struct EventPath {
    v8::Global<v8::Object> invocationTarget;
    v8::Global<v8::Object> shadowAdjustedTarget;
    v8::Global<v8::Object> relatedTarget;
    bool invocationTargetInShadowTree;
    bool rootOfClosedTree;
    bool slotInClosedTree;
};

class Event {
public:
    Event(v8::Local<v8::Object> obj) : weakObject(obj, this), internalField(this) {
        path.reserve(2);
    }

    WeakObject<Event> weakObject;
    InternalField<Event> internalField;
    BString type;
    v8::Global<v8::Object> target;
    v8::Global<v8::Object> currentTarget;
    std::vector<EventPath> path;
    double timeStamp;
    int eventPhase{NONE};
    bool bubbles{false};
    bool cancelable{false};
    bool isTrusted{false};
    bool stopPropagationFlag{false};
    bool stopImmediatePropagationFlag{false};
    bool canceledFlag{false};
    bool inPassiveListenerFlag{false};
    bool composedFlag{false};
    bool initializedFlag{false};
    bool dispatchFlag{false};

    enum {
        NONE = 0,
        CAPTURING_PHASE,
        AT_TARGET,
        BUBBLING_PHASE
    };
};

void exposeEvent(v8::Local<v8::Context> context, ExposedScope exposedScope);

}

#endif
