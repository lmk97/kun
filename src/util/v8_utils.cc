#include "util/v8_utils.h"

#include "util/constants.h"
#include "util/utils.h"

KUN_V8_USINGS;

using v8::Exception;
using v8::Message;
using v8::PropertyDescriptor;
using v8::StackTrace;
using v8::SymbolObject;
using kun::BString;
using kun::util::fromObject;
using kun::util::toV8String;

namespace {

bool findClassNameAndRecv(
    Local<Context> context,
    const BString& name,
    BString& className,
    Local<Object>& recv
) {
    auto obj = context->Global();
    size_t prev = 0;
    auto index = name.find(".");
    while (index != BString::END) {
        prev = index + 1;
        auto key = name.substring(prev, index);
        Local<Object> out;
        if (!fromObject(context, obj, key, out)) {
            return false;
        }
        obj = out;
        index = name.find(".", prev);
    }
    className = name.substring(prev);
    recv = obj;
    return true;
}

}

namespace kun::util {

BString toBString(Local<Context> context, Local<Value> value) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    BString result;
    if (value.IsEmpty() || value->IsExternal()) {
        return result;
    }
    if (value->IsSymbol() || value->IsSymbolObject()) {
        Local<Value> desc;
        if (value->IsSymbol()) {
            desc = value.As<Symbol>()->Description(isolate);
        } else {
            auto symbol = value.As<SymbolObject>()->ValueOf();
            desc = symbol->Description(isolate);
        }
        if (!desc.IsEmpty() && !desc->IsUndefined()) {
            auto v8Str = desc.As<String>();
            auto len = 8 + v8Str->Utf8Length(isolate);
            result.reserve(len);
            result += "Symbol(";
            v8Str->WriteUtf8(isolate, result.data() + 7);
            result.resize(len - 1);
            result += ")";
        } else {
            result += "Symbol()";
        }
    } else {
        Local<String> v8Str;
        if (value->ToString(context).ToLocal(&v8Str)) {
            auto len = v8Str->Utf8Length(isolate);
            result.reserve(len);
            v8Str->WriteUtf8(isolate, result.data());
            result.resize(len);
        }
    }
    return result;
}

BString formatSourceLine(Local<Context> context, Local<Message> message) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    BString result;
    auto column = message->GetStartColumn(context).FromMaybe(-1);
    Local<String> v8Str;
    if (column != -1 && message->GetSourceLine(context).ToLocal(&v8Str)) {
        auto len = v8Str->Utf8Length(isolate);
        result.reserve((len << 1) + 15);
        v8Str->WriteUtf8(isolate, result.data());
        result.resize(len);
        result += "\n";
        for (decltype(column) i = 0; i < column; i++) {
            result += " ";
        }
        result += "\x1b[0;31m^\x1b[0m";
    }
    return result;
}

BString formatStackTrace(Local<Context> context, Local<StackTrace> stackTrace) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    BString result;
    const auto frameCount = stackTrace->GetFrameCount();
    if (stackTrace.IsEmpty() || frameCount <= 0) {
        return result;
    }
    result.reserve(128 * frameCount);
    for (int i = 0; i < frameCount; i++) {
        auto frame = stackTrace->GetFrame(isolate, i);
        auto scriptName = frame->GetScriptNameOrSourceURL();
        auto path = toBString(context, scriptName);
        auto line = frame->GetLineNumber();
        auto column = frame->GetColumn();
        if (!result.empty()) {
            result += "\n";
        }
        result += formatStackTrace(path, line, column);
    }
    return result;
}

BString formatException(Local<Context> context, Local<Value> exception) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    BString result;
    if (exception.IsEmpty()) {
        return result;
    }
    result.reserve(1023);
    result += "\x1b[0;31m";
    if (!exception->IsNativeError()) {
        result += "Uncaught (in promise)\x1b[0m: ";
        result += toBString(context, exception);
        return result;
    }
    auto obj = exception.As<Object>();
    if (BString str; fromObject(context, obj, "name", str)) {
        result += str;
    } else {
        result += "Uncaught (in promise)";
    }
    result += "\x1b[0m: ";
    if (BString str; fromObject(context, obj, "message", str)) {
        result += str;
    } else {
        result += toBString(context, exception);
    }
    auto message = Exception::CreateMessage(isolate, exception);
    auto stackTrace = formatStackTrace(context, Exception::GetStackTrace(exception));
    if (stackTrace.empty()) {
        auto line = message->GetLineNumber(context).FromMaybe(-1);
        auto column = message->GetStartColumn(context).FromMaybe(-1);
        if (line > 0 && column != -1) {
            auto path = toBString(context, message->GetScriptResourceName());
            stackTrace = formatStackTrace(path, line, column + 1);
        }
    }
    if (!stackTrace.empty()) {
        auto sourceLine = formatSourceLine(context, message);
        if (!sourceLine.empty()) {
            result += "\n";
            result += sourceLine;
        }
        result += "\n";
        result += stackTrace;
    }
    return result;
}

bool instanceOf(Local<Context> context, Local<Value> value, const BString& name) {
    if (value->IsNullOrUndefined() || !value->IsObject()) {
        return false;
    }
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    BString className;
    Local<Object> recv;
    if (!findClassNameAndRecv(context, name, className, recv)) {
        return false;
    }
    Local<Function> func;
    if (fromObject(context, recv, className, func)) {
        return value->InstanceOf(context, func).FromMaybe(false);
    }
    return false;
}

void defineAccessor(
    Local<Context> context,
    Local<Object> obj,
    const BString& name,
    const AccessorDescriptor& desc
) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    auto property = toV8String(isolate, name);
    Local<Value> get;
    if (desc.get != nullptr) {
        Local<Function> func;
        if (Function::New(context, desc.get, property).ToLocal(&func)) {
            auto str = BString::format("get {}", name);
            auto v8Str = toV8String(isolate, str);
            func->SetName(v8Str);
            get = func;
        } else {
            KUN_LOG_ERR("invalid 'get' function");
            return;
        }
    } else {
        get = v8::Undefined(isolate);
    }
    Local<Value> set;
    if (desc.set != nullptr) {
        Local<Function> func;
        if (Function::New(context, desc.set, property).ToLocal(&func)) {
            auto str = BString::format("set {}", name);
            auto v8Str = toV8String(isolate, str);
            func->SetName(v8Str);
            set = func;
        } else {
            KUN_LOG_ERR("invalid 'set' function");
            return;
        }
    } else {
        set = v8::Undefined(isolate);
    }
    PropertyDescriptor propDesc(get, set);
    propDesc.set_configurable(desc.configurable);
    propDesc.set_enumerable(desc.enumerable);
    obj->DefineProperty(context, property, propDesc).Check();
}

void inherit(Local<Context> context, Local<Function> child, const BString& parentName) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    BString className;
    Local<Object> recv;
    if (findClassNameAndRecv(context, parentName, className, recv)) {
        Local<Function> parent;
        if (fromObject(context, recv, className, parent)) {
            Local<Object> parentProto;
            Local<Object> childProto;
            if (
                getPrototypeOf(context, parent).ToLocal(&parentProto) &&
                getPrototypeOf(context, child).ToLocal(&childProto)
            ) {
                childProto->SetPrototype(context, parentProto).Check();
                return;
            }
        }
    }
    KUN_LOG_ERR("Could not inherit from '{}'", parentName);
}

MaybeLocal<Object> createObject(
    Local<Context> context,
    const BString& name,
    int fieldCount
) {
    auto isolate = context->GetIsolate();
    EscapableHandleScope handleScope(isolate);
    BString className;
    Local<Object> recv;
    if (!findClassNameAndRecv(context, name, className, recv)) {
        return MaybeLocal<Object>();
    }
    Local<Function> constructor;
    if (!fromObject(context, recv, className, constructor)) {
        return MaybeLocal<Object>();
    }
    Local<Object> proto;
    if (getPrototypeOf(context, constructor).ToLocal(&proto)) {
        auto objTmpl = ObjectTemplate::New(isolate);
        if (fieldCount > 0) {
            objTmpl->SetInternalFieldCount(fieldCount);
        }
        auto obj = objTmpl->NewInstance(context).ToLocalChecked();
        obj->SetPrototype(context, proto).Check();
        return handleScope.Escape(obj);
    }
    return MaybeLocal<Object>();
}

}
