#include "web/dom_exception.h"

#include <stddef.h>

#include <unordered_map>

#include "util/bstring.h"
#include "util/js_utils.h"
#include "util/v8_utils.h"

KUN_V8_USINGS;

using kun::BString;
using kun::BStringHash;
using kun::JS;
using kun::util::checkFuncArgs;
using kun::util::defineAccessor;
using kun::util::fromInternal;
using kun::util::getPrototypeOf;
using kun::util::setToStringTag;
using kun::util::throwTypeError;
using kun::util::toBString;
using kun::util::toV8String;

namespace {

const char* PROPERTIES[] = {
    "INDEX_SIZE_ERR",
    "DOMSTRING_SIZE_ERR",
    "HIERARCHY_REQUEST_ERR",
    "WRONG_DOCUMENT_ERR",
    "INVALID_CHARACTER_ERR",
    "NO_DATA_ALLOWED_ERR",
    "NO_MODIFICATION_ALLOWED_ERR",
    "NOT_FOUND_ERR",
    "NOT_SUPPORTED_ERR",
    "INUSE_ATTRIBUTE_ERR",
    "INVALID_STATE_ERR",
    "SYNTAX_ERR",
    "INVALID_MODIFICATION_ERR",
    "NAMESPACE_ERR",
    "INVALID_ACCESS_ERR",
    "VALIDATION_ERR",
    "TYPE_MISMATCH_ERR",
    "SECURITY_ERR",
    "NETWORK_ERR",
    "ABORT_ERR",
    "URL_MISMATCH_ERR",
    "QUOTA_EXCEEDED_ERR",
    "TIMEOUT_ERR",
    "INVALID_NODE_TYPE_ERR",
    "DATA_CLONE_ERR"
};

std::unordered_map<BString, int, BStringHash> NAME_CODE_MAP = {
    {"IndexSizeError", 1},
    {"HierarchyRequestError", 3},
    {"WrongDocumentError", 4},
    {"InvalidCharacterError", 5},
    {"NoModificationAllowedError", 7},
    {"NotFoundError", 8},
    {"NotSupportedError", 9},
    {"InUseAttributeError", 10},
    {"InvalidStateError", 11},
    {"SyntaxError", 12},
    {"InvalidModificationError", 13},
    {"NamespaceError", 14},
    {"InvalidAccessError", 15},
    {"TypeMismatchError", 17},
    {"SecurityError", 18},
    {"NetworkError", 19},
    {"AbortError", 20},
    {"URLMismatchError", 21},
    {"QuotaExceededError", 22},
    {"TimeoutError", 23},
    {"InvalidNodeTypeError", 24},
    {"DataCloneError", 25}
};

void newDOMException(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    if (!info.IsConstructCall()) {
        throwTypeError(isolate, "Please use the 'new' operator");
        return;
    }
    if (
        !checkFuncArgs<
        JS::Optional | JS::Any,
        JS::Optional | JS::Any
        >(info)
    ) {
        return;
    }
    auto context = isolate->GetCurrentContext();
    const auto len = info.Length();
    auto message = String::Empty(isolate);
    if (len > 0) {
        Local<String> v8Str;
        if (info[0]->ToString(context).ToLocal(&v8Str)) {
            message = v8Str;
        }
    }
    auto name = String::NewFromUtf8Literal(isolate, "Error");
    if (len > 1) {
        Local<String> v8Str;
        if (info[1]->ToString(context).ToLocal(&v8Str)) {
            name = v8Str;
        }
    }
    auto recv = info.This();
    recv->SetInternalField(0, name);
    recv->SetInternalField(1, message);
}

void getName(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    Local<String> name;
    if (fromInternal(recv, 0, name)) {
        info.GetReturnValue().Set(name);
    }
}

void getMessage(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    Local<String> message;
    if (fromInternal(recv, 1, message)) {
        info.GetReturnValue().Set(message);
    }
}

void getCode(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    BString name;
    if (fromInternal(recv, 0, name)) {
        auto iter = NAME_CODE_MAP.find(name);
        auto code = iter != NAME_CODE_MAP.end() ? iter->second : 0;
        info.GetReturnValue().Set(code);
    }
}

}

namespace kun::web {

void exposeDOMException(Local<Context> context, ExposedScope exposedScope) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    auto funcTmpl = FunctionTemplate::New(isolate);
    auto protoTmpl = funcTmpl->PrototypeTemplate();
    auto instTmpl = funcTmpl->InstanceTemplate();
    instTmpl->SetInternalFieldCount(2);
    auto exposedName = toV8String(isolate, "DOMException");
    funcTmpl->SetClassName(exposedName);
    funcTmpl->SetCallHandler(newDOMException);
    setToStringTag(isolate, protoTmpl, exposedName);
    auto func = funcTmpl->GetFunction(context).ToLocalChecked();
    auto proto = getPrototypeOf(context, func).ToLocalChecked();
    defineAccessor(context, proto, "name", {getName});
    defineAccessor(context, proto, "message", {getMessage});
    defineAccessor(context, proto, "code", {getCode});
    for (size_t i = 0; i < sizeof(PROPERTIES) / sizeof(const char*); i++) {
        auto name = String::NewFromUtf8(isolate, PROPERTIES[i]).ToLocalChecked();
        auto value = Number::New(isolate, static_cast<double>(i + 1));
        func->DefineOwnProperty(
            context,
            name,
            value,
            static_cast<PropertyAttribute>(v8::DontDelete | v8::ReadOnly)
        ).Check();
        proto->DefineOwnProperty(
            context,
            name,
            value,
            static_cast<PropertyAttribute>(v8::DontDelete | v8::ReadOnly)
        ).Check();
    }
    auto globalThis = context->Global();
    globalThis->DefineOwnProperty(context, exposedName, func, v8::DontEnum).Check();
}

}
