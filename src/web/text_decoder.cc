#include "web/text_decoder.h"

#include <stddef.h>
#include <stdint.h>

#include "util/js_utils.h"
#include "util/result.h"
#include "util/sys_err.h"
#include "util/v8_utils.h"

KUN_V8_USINGS;

using v8::ArrayBufferView;
using kun::BString;
using kun::Environment;
using kun::InternalField;
using kun::JS;
using kun::Result;
using kun::SysErr;
using kun::web::TextDecoder;
using kun::util::checkFuncArgs;
using kun::util::defineAccessor;
using kun::util::fromInternal;
using kun::util::fromObject;
using kun::util::getPrototypeOf;
using kun::util::setFunction;
using kun::util::setToStringTag;
using kun::util::throwRangeError;
using kun::util::throwTypeError;
using kun::util::toBString;
using kun::util::toV8String;

namespace {

Result<size_t> decodeUtf8(BString& result, const BString& str, const BString& errorMode) {
    const auto fatal = errorMode == "fatal";
    size_t bytesRead = 0;
    int bytesSeen = 0;
    int bytesNeeded = 0;
    int lowerBoundary = 0x80;
    int upperBoundary = 0xbf;
    auto p = str.data();
    auto end = p + str.length();
    while (p < end) {
        const unsigned char u = *p++;
        if (bytesNeeded == 0) {
            if (u <= 0x7f) {
                char s[] = {static_cast<char>(u)};
                result.append(s, 1);
                bytesRead += 1;
                continue;
            }
            if (u >= 0xc2 && u <= 0xdf) {
                bytesNeeded = 1;
            } else if (u >= 0xe0 && u <= 0xef) {
                if (u == 0xe0) {
                    lowerBoundary = 0xa0;
                }
                if (u == 0xed) {
                    upperBoundary = 0x9f;
                }
                bytesNeeded = 2;
            } else if (u >= 0xf0 && u <= 0xf4) {
                if (u == 0xf0) {
                    lowerBoundary = 0x90;
                }
                if (u == 0xf4) {
                    upperBoundary = 0x8f;
                }
                bytesNeeded = 3;
            } else {
                if (fatal) {
                    return SysErr(SysErr::INVALID_CHARSET);
                }
                result += "\xef\xbf\xbd";
            }
            continue;
        }
        if (u < lowerBoundary || u > upperBoundary) {
            bytesRead += bytesSeen + 1;
            bytesNeeded = 0;
            bytesSeen = 0;
            lowerBoundary = 0x80;
            upperBoundary = 0xbf;
            --p;
            if (fatal) {
                return SysErr(SysErr::INVALID_CHARSET);
            }
            result += "\xef\xbf\xbd";
            continue;
        }
        lowerBoundary = 0x80;
        upperBoundary = 0xbf;
        bytesSeen += 1;
        if (bytesSeen != bytesNeeded) {
            continue;
        }
        result.append(p - bytesNeeded - 1, bytesNeeded + 1);
        bytesRead += bytesSeen + 1;
        bytesNeeded = 0;
        bytesSeen = 0;
    }
    return bytesRead;
}

void newTextDecoder(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    if (!info.IsConstructCall()) {
        throwTypeError(isolate, "Please use the 'new' operator");
        return;
    }
    if (
        !checkFuncArgs<
        JS::Optional | JS::Any,
        JS::Optional | JS::Object
        >(info)
    ) {
        return;
    }
    auto context = isolate->GetCurrentContext();
    const auto argNum = info.Length();
    if (argNum > 0) {
        auto label = toBString(context, info[0]);
        if (!label.equalFold("utf-8") && !label.equalFold("utf8")) {
            throwRangeError(isolate, "Only 'utf-8' is supported");
            return;
        }
    }
    bool fatal = false;
    bool ignoreBOM = false;
    if (argNum > 1) {
        auto options = info[1].As<Object>();
        fromObject(context, options, "fatal", fatal);
        fromObject(context, options, "ignoreBOM", ignoreBOM);
    }
    auto env = Environment::from(context);
    auto recv = info.This();
    auto textDecoder = new TextDecoder(env, recv);
    textDecoder->encoding = "utf-8";
    if (fatal) {
        textDecoder->errorMode = "fatal";
    }
    textDecoder->ignoreBOM = ignoreBOM;
}

void decode(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    if (
        !checkFuncArgs<
        JS::Optional | JS::ArrayBuffer | JS::TypedArray | JS::DataView,
        JS::Optional | JS::Object
        >(info)
    ) {
        return;
    }
    auto context = isolate->GetCurrentContext();
    const auto argNum = info.Length();
    char* data = nullptr;
    size_t nbytes = 0;
    if (argNum > 0) {
        if (info[0]->IsArrayBuffer()) {
            auto arrBuf = info[0].As<ArrayBuffer>();
            data = static_cast<char*>(arrBuf->Data());
            nbytes = arrBuf->ByteLength();
        } else {
            auto abv = info[0].As<ArrayBufferView>();
            auto arrBuf = abv->Buffer();
            auto offset = abv->ByteOffset();
            data = static_cast<char*>(arrBuf->Data()) + offset;
            nbytes = abv->ByteLength();
        }
    }
    bool stream = false;
    if (argNum > 1) {
        auto options = info[1].As<Object>();
        fromObject(context, options, "stream", stream);
    }
    auto recv = info.This();
    auto textDecoder = InternalField<TextDecoder>::get(recv, 0);
    if (textDecoder == nullptr) {
        return;
    }
    if (!textDecoder->doNotFlush) {
        textDecoder->ioQueue.resize(0);
    }
    textDecoder->doNotFlush = stream;
    if (data == nullptr || nbytes == 0) {
        info.GetReturnValue().Set(String::Empty(isolate));
        return;
    }
    const auto& errorMode = textDecoder->errorMode;
    auto& ioQueue = textDecoder->ioQueue;
    BString result;
    result.reserve(nbytes + (nbytes >> 2));
    auto p = data;
    auto end = data + nbytes;
    if (!ioQueue.empty()) {
        const auto ioQueueLen = ioQueue.length();
        BString str;
        str += ioQueue;
        str.append(data, nbytes >= 8 ? 8 : nbytes);
        ioQueue.resize(0);
        if (auto r = decodeUtf8(result, str, errorMode)) {
            p += r.unwrap() - ioQueueLen;
        } else {
            throwTypeError(isolate, "The encoded data is invalid");
            return;
        }
    }
    if (p < end) {
        auto str = BString::view(p, end - p);
        if (auto r = decodeUtf8(result, str, errorMode)) {
            p += r.unwrap();
        } else {
            throwTypeError(isolate, "The encoded data is invalid");
            return;
        }
    }
    if (p < end) {
        ioQueue.append(p, end - p);
    }
    auto v8Str = toV8String(isolate, result);
    info.GetReturnValue().Set(v8Str);
}

void getEncoding(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto textDecoder = InternalField<TextDecoder>::get(recv, 0);
    if (textDecoder == nullptr) {
        return;
    }
    auto encoding = toV8String(isolate, textDecoder->encoding);
    info.GetReturnValue().Set(encoding);
}

void getFatal(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto textDecoder = InternalField<TextDecoder>::get(recv, 0);
    if (textDecoder == nullptr) {
        return;
    }
    auto fatal = textDecoder->errorMode == "fatal";
    info.GetReturnValue().Set(fatal);
}

void getIgnoreBOM(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto recv = info.This();
    auto textDecoder = InternalField<TextDecoder>::get(recv, 0);
    if (textDecoder == nullptr) {
        return;
    }
    info.GetReturnValue().Set(textDecoder->ignoreBOM);
}

}

namespace kun::web {

void exposeTextDecoder(Local<Context> context, ExposedScope exposedScope) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    auto funcTmpl = FunctionTemplate::New(isolate);
    auto protoTmpl = funcTmpl->PrototypeTemplate();
    auto instTmpl = funcTmpl->InstanceTemplate();
    instTmpl->SetInternalFieldCount(1);
    auto exposedName = toV8String(isolate, "TextDecoder");
    funcTmpl->SetClassName(exposedName);
    funcTmpl->SetCallHandler(newTextDecoder);
    setToStringTag(isolate, protoTmpl, exposedName);
    setFunction(isolate, protoTmpl, "decode", decode);
    auto func = funcTmpl->GetFunction(context).ToLocalChecked();
    auto proto = getPrototypeOf(context, func).ToLocalChecked();
    defineAccessor(context, proto, "encoding", {getEncoding});
    defineAccessor(context, proto, "fatal", {getFatal});
    defineAccessor(context, proto, "ignoreBOM", {getIgnoreBOM});
    auto globalThis = context->Global();
    globalThis->DefineOwnProperty(context, exposedName, func, v8::DontEnum).Check();
}

}
