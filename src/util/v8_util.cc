#include "util/v8_util.h"

#include <stddef.h>

#include "util/constants.h"

KUN_V8_USINGS;

using v8::Exception;
using v8::Message;
using v8::StackTrace;
using v8::SymbolObject;

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
            auto v8str = desc.As<String>();
            auto len = static_cast<size_t>(8 + v8str->Utf8Length(isolate));
            result.reserve(len);
            result += "Symbol(";
            v8str->WriteUtf8(isolate, result.data() + 7);
            result.resize(len - 1);
            result += ")";
        } else {
            result += "Symbol()";
        }
    } else {
        Local<String> v8str;
        if (value->ToString(context).ToLocal(&v8str)) {
            auto len = static_cast<size_t>(v8str->Utf8Length(isolate));
            result.reserve(len);
            v8str->WriteUtf8(isolate, result.data());
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
    Local<String> v8str;
    if (column != -1 && message->GetSourceLine(context).ToLocal(&v8str)) {
        auto len = static_cast<size_t>(v8str->Utf8Length(isolate));
        result.reserve((len << 1) + 15);
        v8str->WriteUtf8(isolate, result.data());
        result.resize(len);
        result += "\n";
        for (decltype(column) i = 0; i < column; i++) {
            result += " ";
        }
        result += "\033[0;31m^\033[0m";
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
        auto path = toBString(context, frame->GetScriptNameOrSourceURL());
        auto line = frame->GetLineNumber();
        auto column = frame->GetColumn();
        if (!result.empty()) {
            result += "\n";
        }
        result += BString::format(
            "    at \033[0;36m{}\033[0m:\033[0;33m{}\033[0m:\033[0;33m{}\033[0m",
            path, line, column
        );
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
    result += "\033[0;31m";
    if (!exception->IsNativeError()) {
        result += "Uncaught (in promise)\033[0m: ";
        result += toBString(context, exception);
        return result;
    }
    auto obj = exception.As<Object>();
    Local<Value> value;
    if (obj->Get(context, toV8String(isolate, "name")).ToLocal(&value)) {
        result += toBString(context, value);
    } else {
        result += "Uncaught (in promise)";
    }
    result += "\033[0m: ";
    if (obj->Get(context, toV8String(isolate, "message")).ToLocal(&value)) {
        result += toBString(context, value);
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
            stackTrace = BString::format(
                "    at \033[0;36m{}\033[0m:\033[0;33m{}\033[0m:\033[0;33m{}\033[0m",
                path, line, column + 1
            );
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

}
