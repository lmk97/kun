#include "web/event_target.h"

#include "util/js_utils.h"
#include "util/utils.h"
#include "util/v8_utils.h"
#include "web/abort_signal.h"

KUN_V8_USINGS;

using v8::Name;
using kun::BString;
using kun::Environment;
using kun::InternalField;
using kun::JS;
using kun::web::Event;
using kun::web::EventListener;
using kun::web::EventPath;
using kun::web::EventTarget;
using kun::util::checkFuncArgs;
using kun::util::fromObject;
using kun::util::inObject;
using kun::util::instanceOf;
using kun::util::newInstance;
using kun::util::setFunction;
using kun::util::setToStringTag;
using kun::util::throwTypeError;
using kun::util::toBString;
using kun::util::toV8String;

namespace {

void flattenOptions(Local<Context> context, Local<Value> value, EventListener& listener) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    if (value->IsObject()) {
        auto options = value.As<Object>();
        bool capture = false;
        if (fromObject(context, options, "capture", capture)) {
            listener.capture = capture;
        }
        bool once = false;
        if (fromObject(context, options, "once", once)) {
            listener.once = once;
        }
        bool passive = false;
        if (fromObject(context, options, "passive", passive)) {
            listener.passive = passive;
        }
        if (inObject(context, options, "signal")) {
            Local<Object> signal;
            if (
                fromObject(context, options, "signal", signal) &&
                instanceOf(context, signal, "AbortSignal")
            ) {
                listener.signal.Reset(isolate, signal);
            } else {
                throwTypeError(isolate, "Failed to convert value to 'AbortSignal'");
            }
        }
    } else {
        listener.capture = value->BooleanValue(isolate);
    }
}

void abortStepsCallback(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto context = isolate->GetCurrentContext();
    auto data = info.Data();
    if (data.IsEmpty() || data->IsNull() || !data->IsObject()) {
        return;
    }
    auto obj = data.As<Object>();
    Local<Object> target;
    if (!fromObject(context, obj, "target", target)) {
        return;
    }
    BString type;
    if (!fromObject(context, obj, "type", type)) {
        return;
    }
    Local<Object> callback;
    if (!fromObject(context, obj, "callback", callback)) {
        return;
    }
    bool capture;
    if (!fromObject(context, obj, "capture", capture)) {
        return;
    }
    auto eventTarget = InternalField<EventTarget>::get(target, 0);
    if (eventTarget == nullptr) {
        return;
    }
    EventListener listener;
    listener.callback.Reset(isolate, callback);
    listener.capture = capture;
    eventTarget->removeEventListener(type, listener);
}

Local<Function> abortSteps(
    Local<Context> context,
    Local<Object> target,
    const BString& type,
    Local<Object> callback,
    bool capture
) {
    auto isolate = context->GetIsolate();
    EscapableHandleScope handleScope(isolate);
    Local<Name> names[] = {
        toV8String(isolate, "target"),
        toV8String(isolate, "type"),
        toV8String(isolate, "callback"),
        toV8String(isolate, "capture")
    };
    Local<Value> values[] = {
        target,
        toV8String(isolate, type),
        callback,
        Boolean::New(isolate, capture)
    };
    auto data = Object::New(isolate, v8::Null(isolate), names, values, 4);
    auto func = Function::New(context, abortStepsCallback, data).ToLocalChecked();
    return handleScope.Escape(func);
}

void appendToEventPath(
    Isolate* isolate,
    Event* event,
    Local<Value> invocationTarget,
    Local<Value> shadowAdjustedTarget,
    Local<Value> relatedTarget,
    bool slotInClosedTree
) {
    EventPath eventPath;
    if (!invocationTarget->IsNull() && invocationTarget->IsObject()) {
        auto obj = invocationTarget.As<Object>();
        eventPath.invocationTarget.Reset(isolate, obj);
    }
    eventPath.invocationTargetInShadowTree = false;
    if (!shadowAdjustedTarget->IsNull() && shadowAdjustedTarget->IsObject()) {
        auto obj = shadowAdjustedTarget.As<Object>();
        eventPath.shadowAdjustedTarget.Reset(isolate, obj);
    }
    if (!relatedTarget->IsNull() && relatedTarget->IsObject()) {
        auto obj = relatedTarget.As<Object>();
        eventPath.relatedTarget.Reset(isolate, obj);
    }
    eventPath.rootOfClosedTree = false;
    eventPath.slotInClosedTree = slotInClosedTree;
    event->path.emplace_back(std::move(eventPath));
}

bool innerInvokeEventListeners(
    Local<Context> context,
    Event* event,
    std::multimap<BString, EventListener>& listeners,
    const BString& phase
) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    bool found = false;
    auto [begin, end] = listeners.equal_range(event->type);
    for (auto iter = begin; iter != end; ++iter) {
        auto& listener = iter->second;
        if (listener.removed) {
            continue;
        }
        found = true;
        if (
            (phase == "capturing" && !listener.capture) ||
            (phase == "bubbling" && listener.capture)
        ) {
            continue;
        }
        if (listener.once) {
            listener.removed = true;
        }
        if (listener.passive) {
            event->inPassiveListenerFlag = true;
        }
        auto callback = listener.callback.Get(isolate);
        Local<Value> recv;
        Local<Function> func;
        if (callback->IsFunction()) {
            if (event->currentTarget.IsEmpty()) {
                recv = v8::Undefined(isolate);
            } else {
                recv = event->currentTarget.Get(isolate);
            }
            func = callback.As<Function>();
        } else {
            recv = callback;
            auto obj = callback.As<Object>();
            if (!fromObject(context, obj, "handleEvent", func)) {
                KUN_LOG_ERR("'handleEvent' not found");
                continue;
            }
        }
        auto eventObj = event->weakObject.get();
        Local<Value> argv[] = {eventObj};
        Local<Value> result;
        if (!func->Call(context, recv, 1, argv).ToLocal(&result)) {
            KUN_LOG_ERR("EventListener callback failed");
        }
        event->inPassiveListenerFlag = false;
        if (event->stopImmediatePropagationFlag) {
            return found;
        }
    }
    return found;
}

void invokeEventListeners(
    Local<Context> context,
    Event* event,
    const EventPath& eventPath,
    const BString& phase
) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    if (eventPath.shadowAdjustedTarget.IsEmpty()) {
        bool found = false;
        const auto& path = event->path;
        for (auto iter = path.crbegin(); iter != path.crend(); ++iter) {
            const auto& ep = *iter;
            if (!found && &eventPath != &ep) {
                continue;
            }
            found = true;
            if (!ep.shadowAdjustedTarget.IsEmpty()) {
                auto target = ep.shadowAdjustedTarget.Get(isolate);
                event->target.Reset(isolate, target);
                break;
            }
        }
    } else {
        auto target = eventPath.shadowAdjustedTarget.Get(isolate);
        event->target.Reset(isolate, target);
    }
    if (!eventPath.relatedTarget.IsEmpty()) {
        auto target = eventPath.relatedTarget.Get(isolate);
        event->relatedTarget.Reset(isolate, target);
    }
    if (event->stopPropagationFlag) {
        return;
    }
    if (!eventPath.invocationTarget.IsEmpty()) {
        auto target = eventPath.invocationTarget.Get(isolate);
        event->currentTarget.Reset(isolate, target);
        auto eventTarget = InternalField<EventTarget>::get(target, 0);
        if (eventTarget == nullptr) {
            return;
        }
        auto& listeners = eventTarget->listeners;
        innerInvokeEventListeners(context, event, listeners, phase);
    }
}

void newEventTarget(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    if (!info.IsConstructCall()) {
        throwTypeError(isolate, "Please use the 'new' operator");
        return;
    }
    auto context = isolate->GetCurrentContext();
    auto env = Environment::from(context);
    auto recv = info.This();
    new EventTarget(env, recv);
}

void addEventListener(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    if (
        !checkFuncArgs<
        JS::Any,
        JS::Optional | JS::Undefined | JS::Null | JS::Object,
        JS::Optional | JS::Any
        >(info)
    ) {
        return;
    }
    const auto len = info.Length();
    if (len < 2 || info[1]->IsNullOrUndefined()) {
        return;
    }
    auto context = isolate->GetCurrentContext();
    EventListener listener;
    if (info[1]->IsFunction()) {
        listener.callback.Reset(isolate, info[1].As<Function>());
    } else {
        auto obj = info[1].As<Object>();
        Local<Function> func;
        if (fromObject(context, obj, "handleEvent", func)) {
            listener.callback.Reset(isolate, obj);
        } else {
            return;
        }
    }
    auto type = toBString(context, info[0]);
    listener.passive = false;
    if (len > 2) {
        flattenOptions(context, info[2], listener);
    }
    auto recv = info.This();
    auto eventTarget = InternalField<EventTarget>::get(recv, 0);
    if (eventTarget == nullptr) {
        return;
    }
    eventTarget->addEventListener(type, std::move(listener));
}

void removeEventListener(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    if (
        !checkFuncArgs<
        JS::Any,
        JS::Optional | JS::Undefined | JS::Null | JS::Object,
        JS::Optional | JS::Any
        >(info)
    ) {
        return;
    }
    const auto len = info.Length();
    if (len < 2 || info[1]->IsNullOrUndefined()) {
        return;
    }
    auto context = isolate->GetCurrentContext();
    auto callback = info[1].As<Object>();
    if (!callback->IsFunction()) {
        Local<Function> func;
        if (!fromObject(context, callback, "handleEvent", func)) {
            return;
        }
    }
    bool capture = false;
    if (len > 2) {
        if (info[2]->IsObject()) {
            auto options = info[2].As<Object>();
            fromObject(context, options, "capture", capture);
        } else {
            capture = info[2]->BooleanValue(isolate);
        }
    }
    auto recv = info.This();
    auto eventTarget = InternalField<EventTarget>::get(recv, 0);
    if (eventTarget == nullptr) {
        return;
    }
    auto type = toBString(context, info[0]);
    EventListener listener;
    listener.callback.Reset(isolate, callback);
    listener.capture = capture;
    eventTarget->removeEventListener(type, listener);
}

void dispatchEvent(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    if (!checkFuncArgs<JS::Object>(info)) {
        return;
    }
    auto context = isolate->GetCurrentContext();
    if (!instanceOf(context, info[0], "Event")) {
        throwTypeError(isolate, "parameter 1 is not of type 'Event'");
        return;
    }
    auto obj = info[0].As<Object>();
    auto event = InternalField<Event>::get(obj, 0);
    if (event == nullptr) {
        return;
    }
    auto recv = info.This();
    auto eventTarget = InternalField<EventTarget>::get(recv, 0);
    if (eventTarget == nullptr) {
        return;
    }
    auto dispatched = eventTarget->dispatchEvent(event);
    info.GetReturnValue().Set(dispatched);
}

}

namespace kun::web {

EventListener* EventTarget::addEventListener(const BString& type, EventListener&& listener) {
    auto isolate = env->getIsolate();
    HandleScope handleScope(isolate);
    auto context = env->getContext();
    AbortSignal* abortSignal = nullptr;
    if (!listener.signal.IsEmpty()) {
        auto signal = listener.signal.Get(isolate);
        abortSignal = InternalField<AbortSignal>::get(signal, 0);
        if (abortSignal != nullptr && abortSignal->isAborted()) {
            return nullptr;
        }
    }
    if (listener.callback.IsEmpty()) {
        return nullptr;
    }
    auto callback = listener.callback.Get(isolate);
    auto capture = listener.capture;
    auto [begin, end] = listeners.equal_range(type);
    for (auto iter = begin; iter != end; ++iter) {
        const auto& t = iter->second;
        auto cb = t.callback.Get(isolate);
        if (callback->StrictEquals(cb) && capture == t.capture) {
            return nullptr;
        }
    }
    auto iter = listeners.emplace(type, std::move(listener));
    if (abortSignal != nullptr) {
        auto target = weakObject.get();
        auto func = abortSteps(context, target, type, callback, capture);
        abortSignal->addAlgorithm(func);
    }
    return &iter->second;
}

void EventTarget::removeEventListener(const BString& type, const EventListener& listener) {
    auto isolate = env->getIsolate();
    HandleScope handleScope(isolate);
    if (listener.callback.IsEmpty()) {
        return;
    }
    auto callback = listener.callback.Get(isolate);
    auto capture = listener.capture;
    auto [begin, end] = listeners.equal_range(type);
    for (auto iter = begin; iter != end; ++iter) {
        const auto& t = iter->second;
        auto cb = t.callback.Get(isolate);
        if (callback->StrictEquals(cb) && capture == t.capture) {
            listeners.erase(iter);
            break;
        }
    }
}

bool EventTarget::dispatchEvent(Event* event) {
    auto isolate = env->getIsolate();
    HandleScope handleScope(isolate);
    auto context = env->getContext();
    if (event->dispatchFlag || !event->initializedFlag) {
        auto e = newInstance(
            context,
            "DOMException",
            "Invalid event state",
            "InvalidStateError"
        ).ToLocalChecked();
        isolate->ThrowException(e);
        return false;
    }
    event->isTrusted = false;
    event->dispatchFlag = true;
    auto target = weakObject.get();
    Local<Value> relatedTarget;
    if (event->relatedTarget.IsEmpty()) {
        relatedTarget = v8::Null(isolate);
    } else {
        relatedTarget = event->relatedTarget.Get(isolate);
    }
    appendToEventPath(isolate, event, target, target, relatedTarget, false);
    const auto& path = event->path;
    for (auto iter = path.crbegin(); iter != path.crend(); ++iter) {
        const auto& eventPath = *iter;
        if (!eventPath.shadowAdjustedTarget.IsEmpty()) {
            event->eventPhase = Event::AT_TARGET;
        } else {
            event->eventPhase = Event::CAPTURING_PHASE;
        }
        invokeEventListeners(context, event, eventPath, "capturing");
    }
    for (const auto& eventPath : path) {
        if (!eventPath.shadowAdjustedTarget.IsEmpty()) {
            event->eventPhase = Event::AT_TARGET;
        } else {
            if (!event->bubbles) {
                continue;
            }
            event->eventPhase = Event::BUBBLING_PHASE;
        }
        invokeEventListeners(context, event, eventPath, "bubbling");
    }
    event->eventPhase = Event::NONE;
    event->currentTarget.Reset();
    event->path.clear();
    event->dispatchFlag = false;
    event->stopPropagationFlag = false;
    event->stopImmediatePropagationFlag = false;
    return !event->canceledFlag;
}

void exposeEventTarget(Local<Context> context, ExposedScope exposedScope) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    auto funcTmpl = FunctionTemplate::New(isolate);
    auto protoTmpl = funcTmpl->PrototypeTemplate();
    auto instTmpl = funcTmpl->InstanceTemplate();
    instTmpl->SetInternalFieldCount(1);
    auto exposedName = toV8String(isolate, "EventTarget");
    funcTmpl->SetClassName(exposedName);
    funcTmpl->SetCallHandler(newEventTarget);
    setToStringTag(isolate, protoTmpl, exposedName);
    setFunction(isolate, protoTmpl, "addEventListener", addEventListener);
    setFunction(isolate, protoTmpl, "removeEventListener", removeEventListener);
    setFunction(isolate, protoTmpl, "dispatchEvent", dispatchEvent);
    auto func = funcTmpl->GetFunction(context).ToLocalChecked();
    auto globalThis = context->Global();
    globalThis->DefineOwnProperty(context, exposedName, func, v8::DontEnum).Check();
}

}
