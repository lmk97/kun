#include "web/event.h"

#include <list>

#include "sys/time.h"
#include "util/js_utils.h"
#include "util/v8_utils.h"

KUN_V8_USINGS;

using kun::BString;
using kun::InternalField;
using kun::JS;
using kun::WeakObject;
using kun::web::Event;
using kun::sys::microsecond;
using kun::util::checkFuncArgs;
using kun::util::defineAccessor;
using kun::util::fromObject;
using kun::util::getPrototypeOf;
using kun::util::setFunction;
using kun::util::setToStringTag;
using kun::util::throwTypeError;
using kun::util::toBString;
using kun::util::toV8String;

namespace {

void getIsTrusted(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto event = InternalField<Event>::get(recv, 0);
    if (event == nullptr) {
        return;
    }
    info.GetReturnValue().Set(event->isTrusted);
}

void newEvent(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    if (!info.IsConstructCall()) {
        throwTypeError(isolate, "Please use the 'new' operator");
        return;
    }
    if (
        !checkFuncArgs<
        JS::Any,
        JS::Optional | JS::Undefined | JS::Null | JS::Object
        >(info)
    ) {
        return;
    }
    auto context = isolate->GetCurrentContext();
    auto recv = info.This();
    defineAccessor(context, recv, "isTrusted", {getIsTrusted, nullptr, false, true});
    auto event = new Event(recv);
    event->initializedFlag = true;
    event->timeStamp = static_cast<double>(microsecond().unwrap()) / 1000;
    event->type = toBString(context, info[0]);
    if (info.Length() > 1 && !info[1]->IsNullOrUndefined()) {
        auto options = info[1].As<Object>();
        bool bubbles = false;
        if (fromObject(context, options, "bubbles", bubbles)) {
            event->bubbles = bubbles;
        }
        bool cancelable = false;
        if (fromObject(context, options, "cancelable", cancelable)) {
            event->cancelable = cancelable;
        }
    }
}

void initEvent(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    if (
        !checkFuncArgs<
        JS::Any,
        JS::Optional | JS::Any,
        JS::Optional | JS::Any
        >(info)
    ) {
        return;
    }
    auto recv = info.This();
    auto event = InternalField<Event>::get(recv, 0);
    if (event == nullptr) {
        return;
    }
    if (event->dispatchFlag) {
        return;
    }
    auto context = isolate->GetCurrentContext();
    event->initializedFlag = true;
    event->stopPropagationFlag = false;
    event->stopImmediatePropagationFlag = false;
    event->canceledFlag = false;
    event->isTrusted = false;
    event->target.Reset();
    event->type = toBString(context, info[0]);
    event->bubbles = false;
    event->cancelable = false;
    const auto len = info.Length();
    if (len > 1) {
        event->bubbles = info[1]->BooleanValue(isolate);
    }
    if (len > 2) {
        event->cancelable = info[2]->BooleanValue(isolate);
    }
}

void getType(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto event = InternalField<Event>::get(recv, 0);
    if (event == nullptr) {
        return;
    }
    auto type = toV8String(isolate, event->type);
    info.GetReturnValue().Set(type);
}

void getTarget(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto event = InternalField<Event>::get(recv, 0);
    if (event == nullptr) {
        return;
    }
    if (event->target.IsEmpty()) {
        info.GetReturnValue().SetNull();
    } else {
        auto target = event->target.Get(isolate);
        info.GetReturnValue().Set(target);
    }
}

void getSrcElement(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto event = InternalField<Event>::get(recv, 0);
    if (event == nullptr) {
        return;
    }
    if (event->target.IsEmpty()) {
        info.GetReturnValue().SetNull();
    } else {
        auto srcElement = event->target.Get(isolate);
        info.GetReturnValue().Set(srcElement);
    }
}

void getCurrentTarget(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto event = InternalField<Event>::get(recv, 0);
    if (event == nullptr) {
        return;
    }
    if (event->currentTarget.IsEmpty()) {
        info.GetReturnValue().SetNull();
    } else {
        auto currentTarget = event->currentTarget.Get(isolate);
        info.GetReturnValue().Set(currentTarget);
    }
}

void composedPath(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto event = InternalField<Event>::get(recv, 0);
    if (event == nullptr) {
        return;
    }
    const auto& path = event->path;
    if (path.empty()) {
        auto arr = Array::New(isolate);
        info.GetReturnValue().Set(arr);
        return;
    }
    const auto pathSize = static_cast<int>(path.size());
    std::list<Local<Value>> list;
    Local<Value> currentTarget;
    if (event->currentTarget.IsEmpty()) {
        currentTarget = v8::Null(isolate);
    } else {
        currentTarget = event->currentTarget.Get(isolate);
    }
    list.emplace_back(currentTarget);
    int currentTargetIndex = 0;
    int currentTargetHiddenSubtreeLevel = 0;
    int index = pathSize - 1;
    while (index >= 0) {
        const auto& eventPath = path[index];
        if (eventPath.rootOfClosedTree) {
            currentTargetHiddenSubtreeLevel += 1;
        }
        if (!eventPath.invocationTarget.IsEmpty()) {
            auto invocationTarget = eventPath.invocationTarget.Get(isolate);
            if (invocationTarget->StrictEquals(currentTarget)) {
                currentTargetIndex = index;
                break;
            }
        }
        if (eventPath.slotInClosedTree) {
            currentTargetHiddenSubtreeLevel -= 1;
        }
        index -= 1;
    }
    auto currentHiddenLevel = currentTargetHiddenSubtreeLevel;
    auto maxHiddenLevel = currentTargetHiddenSubtreeLevel;
    index = currentTargetIndex - 1;
    while (index >= 0) {
        const auto& eventPath = path[index];
        if (eventPath.rootOfClosedTree) {
            currentHiddenLevel += 1;
        }
        if (currentHiddenLevel <= maxHiddenLevel) {
            Local<Value> invocationTarget;
            if (eventPath.invocationTarget.IsEmpty()) {
                invocationTarget = v8::Null(isolate);
            } else {
                invocationTarget = eventPath.invocationTarget.Get(isolate);
            }
            list.emplace_front(invocationTarget);
        }
        if (eventPath.slotInClosedTree) {
            currentHiddenLevel -= 1;
            if (currentHiddenLevel < maxHiddenLevel) {
                maxHiddenLevel = currentHiddenLevel;
            }
        }
        index -= 1;
    }
    currentHiddenLevel = currentTargetHiddenSubtreeLevel;
    maxHiddenLevel = currentTargetHiddenSubtreeLevel;
    index = currentTargetIndex + 1;
    while (index < pathSize) {
        const auto& eventPath = path[index];
        if (eventPath.slotInClosedTree) {
            currentHiddenLevel += 1;
        }
        if (currentHiddenLevel <= maxHiddenLevel) {
            Local<Value> invocationTarget;
            if (eventPath.invocationTarget.IsEmpty()) {
                invocationTarget = v8::Null(isolate);
            } else {
                invocationTarget = eventPath.invocationTarget.Get(isolate);
            }
            list.emplace_back(invocationTarget);
        }
        if (eventPath.rootOfClosedTree) {
            currentHiddenLevel -= 1;
            if (currentHiddenLevel < maxHiddenLevel) {
                maxHiddenLevel = currentHiddenLevel;
            }
        }
        index += 1;
    }
    auto context = isolate->GetCurrentContext();
    auto arr = Array::New(isolate, list.size());
    uint32_t arrIndex = 0;
    for (const auto& value : list) {
        arr->Set(context, arrIndex++, value).Check();
    }
    info.GetReturnValue().Set(arr);
}

void getEventPhase(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto event = InternalField<Event>::get(recv, 0);
    if (event == nullptr) {
        return;
    }
    info.GetReturnValue().Set(event->eventPhase);
}

void getNone(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    info.GetReturnValue().Set(Event::NONE);
}

void getCapturingPhase(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    info.GetReturnValue().Set(Event::CAPTURING_PHASE);
}

void getAtTarget(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    info.GetReturnValue().Set(Event::AT_TARGET);
}

void getBubblingPhase(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    info.GetReturnValue().Set(Event::BUBBLING_PHASE);
}

void stopPropagation(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto event = InternalField<Event>::get(recv, 0);
    if (event == nullptr) {
        return;
    }
    event->stopPropagationFlag = true;
}

void getCancelBubble(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto event = InternalField<Event>::get(recv, 0);
    if (event == nullptr) {
        return;
    }
    info.GetReturnValue().Set(event->stopPropagationFlag);
}

void setCancelBubble(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    if (!checkFuncArgs<JS::Any>(info)) {
        return;
    }
    if (info[0]->BooleanValue(isolate)) {
        auto recv = info.This();
        auto event = InternalField<Event>::get(recv, 0);
        if (event == nullptr) {
            return;
        }
        event->stopPropagationFlag = true;
    }
}

void stopImmediatePropagation(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto event = InternalField<Event>::get(recv, 0);
    if (event == nullptr) {
        return;
    }
    event->stopPropagationFlag = true;
    event->stopImmediatePropagationFlag = true;
}

void getBubbles(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto event = InternalField<Event>::get(recv, 0);
    if (event == nullptr) {
        return;
    }
    info.GetReturnValue().Set(event->bubbles);
}

void getCancelable(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto event = InternalField<Event>::get(recv, 0);
    if (event == nullptr) {
        return;
    }
    info.GetReturnValue().Set(event->cancelable);
}

void getReturnValue(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto event = InternalField<Event>::get(recv, 0);
    if (event == nullptr) {
        return;
    }
    info.GetReturnValue().Set(!event->canceledFlag);
}

void setReturnValue(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    if (!checkFuncArgs<JS::Any>(info)) {
        return;
    }
    if (!info[0]->BooleanValue(isolate)) {
        auto recv = info.This();
        auto event = InternalField<Event>::get(recv, 0);
        if (event == nullptr) {
            return;
        }
        event->canceledFlag = true;
    }
}

void preventDefault(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto event = InternalField<Event>::get(recv, 0);
    if (event == nullptr) {
        return;
    }
    event->canceledFlag = true;
}

void getDefaultPrevented(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto event = InternalField<Event>::get(recv, 0);
    if (event == nullptr) {
        return;
    }
    info.GetReturnValue().Set(event->canceledFlag);
}

void getComposed(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto event = InternalField<Event>::get(recv, 0);
    if (event == nullptr) {
        return;
    }
    info.GetReturnValue().Set(event->composedFlag);
}

void getTimeStamp(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto event = InternalField<Event>::get(recv, 0);
    if (event == nullptr) {
        return;
    }
    info.GetReturnValue().Set(event->timeStamp);
}

}

namespace kun::web {

void exposeEvent(Local<Context> context, ExposedScope exposedScope) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    auto funcTmpl = FunctionTemplate::New(isolate);
    auto protoTmpl = funcTmpl->PrototypeTemplate();
    auto instTmpl = funcTmpl->InstanceTemplate();
    instTmpl->SetInternalFieldCount(1);
    auto exposedName = toV8String(isolate, "Event");
    funcTmpl->SetClassName(exposedName);
    funcTmpl->SetCallHandler(newEvent);
    setToStringTag(isolate, protoTmpl, exposedName);
    setFunction(isolate, protoTmpl, "composedPath", composedPath);
    setFunction(isolate, protoTmpl, "stopPropagation", stopPropagation);
    setFunction(isolate, protoTmpl, "stopImmediatePropagation", stopImmediatePropagation);
    setFunction(isolate, protoTmpl, "preventDefault", preventDefault);
    setFunction(isolate, protoTmpl, "initEvent", initEvent);
    auto func = funcTmpl->GetFunction(context).ToLocalChecked();
    auto proto = getPrototypeOf(context, func).ToLocalChecked();
    defineAccessor(context, proto, "type", {getType});
    defineAccessor(context, proto, "target", {getTarget});
    defineAccessor(context, proto, "srcElement", {getSrcElement});
    defineAccessor(context, proto, "currentTarget", {getCurrentTarget});
    defineAccessor(context, proto, "eventPhase", {getEventPhase});
    defineAccessor(context, proto, "NONE", {getNone});
    defineAccessor(context, proto, "CAPTURING_PHASE", {getCapturingPhase});
    defineAccessor(context, proto, "AT_TARGET", {getAtTarget});
    defineAccessor(context, proto, "BUBBLING_PHASE", {getBubblingPhase});
    defineAccessor(context, proto, "cancelBubble", {getCancelBubble, setCancelBubble});
    defineAccessor(context, proto, "bubbles", {getBubbles});
    defineAccessor(context, proto, "cancelable", {getCancelable});
    defineAccessor(context, proto, "returnValue", {getReturnValue, setReturnValue});
    defineAccessor(context, proto, "defaultPrevented", {getDefaultPrevented});
    defineAccessor(context, proto, "composed", {getComposed});
    defineAccessor(context, proto, "timeStamp", {getTimeStamp});
    func->DefineOwnProperty(
        context,
        toV8String(isolate, "NONE"),
        Number::New(isolate, Event::NONE),
        static_cast<PropertyAttribute>(v8::DontDelete | v8::ReadOnly)
    ).Check();
    func->DefineOwnProperty(
        context,
        toV8String(isolate, "CAPTURING_PHASE"),
        Number::New(isolate, Event::CAPTURING_PHASE),
        static_cast<PropertyAttribute>(v8::DontDelete | v8::ReadOnly)
    ).Check();
    func->DefineOwnProperty(
        context,
        toV8String(isolate, "AT_TARGET"),
        Number::New(isolate, Event::AT_TARGET),
        static_cast<PropertyAttribute>(v8::DontDelete | v8::ReadOnly)
    ).Check();
    func->DefineOwnProperty(
        context,
        toV8String(isolate, "BUBBLING_PHASE"),
        Number::New(isolate, Event::BUBBLING_PHASE),
        static_cast<PropertyAttribute>(v8::DontDelete | v8::ReadOnly)
    ).Check();
    auto globalThis = context->Global();
    globalThis->DefineOwnProperty(context, exposedName, func, v8::DontEnum).Check();
}

}
