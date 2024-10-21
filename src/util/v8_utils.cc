#include "util/v8_utils.h"

#include <stddef.h>

#include "util/constants.h"

KUN_V8_USINGS;

using v8::Exception;
using v8::Message;
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
        auto key = name.substring(prev, index);
        prev = index + 1;
        Local<Object> tmp;
        if (!fromObject(context, obj, key, tmp)) {
            return false;
        }
        obj = tmp;
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
            auto len = static_cast<size_t>(8 + v8Str->Utf8Length(isolate));
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
            auto len = static_cast<size_t>(v8Str->Utf8Length(isolate));
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
        auto len = static_cast<size_t>(v8Str->Utf8Length(isolate));
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
    if (stackTrace.IsEmpty() || stackTrace->GetFrameCount() <= 0) {
        return result;
    }
    result.reserve(1023);
    for (int i = 0; i < stackTrace->GetFrameCount(); i++) {
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

}
