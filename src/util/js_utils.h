#ifndef KUN_UTIL_JS_UTILS_H
#define KUN_UTIL_JS_UTILS_H

#include <stdint.h>

#include "v8.h"
#include "env/environment.h"
#include "loop/async_request.h"
#include "loop/event_loop.h"
#include "util/bstring.h"
#include "util/v8_utils.h"

namespace kun {

class JS {
public:
    JS() = delete;

    ~JS() = delete;

    template<uint32_t N>
    static BString name() {
        std::vector<BString> strs;
        strs.reserve(16);
        if constexpr (N & Optional) {
            strs.emplace_back("optional");
        }
        if constexpr (N & Any) {
            strs.emplace_back("any");
        }
        if constexpr (N & Array) {
            strs.emplace_back("array");
        }
        if constexpr (N & ArrayBuffer) {
            strs.emplace_back("ArrayBuffer");
        }
        if constexpr (N & BigInt) {
            strs.emplace_back("bigint");
        }
        if constexpr (N & Boolean) {
            strs.emplace_back("boolean");
        }
        if constexpr (N & DataView) {
            strs.emplace_back("DataView");
        }
        if constexpr (N & Function) {
            strs.emplace_back("function");
        }
        if constexpr (N & Map) {
            strs.emplace_back("Map");
        }
        if constexpr (N & Null) {
            strs.emplace_back("null");
        }
        if constexpr (N & Number) {
            strs.emplace_back("number");
        }
        if constexpr (N & Object) {
            strs.emplace_back("object");
        }
        if constexpr (N & Promise) {
            strs.emplace_back("Promise");
        }
        if constexpr (N & RegExp) {
            strs.emplace_back("regexp");
        }
        if constexpr (N & Set) {
            strs.emplace_back("Set");
        }
        if constexpr (N & String) {
            strs.emplace_back("string");
        }
        if constexpr (N & Symbol) {
            strs.emplace_back("Symbol");
        }
        if constexpr (N & TypedArray) {
            strs.emplace_back("TypedArray");
        }
        if constexpr (N & Uint8Array) {
            strs.emplace_back("Uint8Array");
        }
        if constexpr (N & Undefined) {
            strs.emplace_back("undefined");
        }
        BString result;
        result.reserve(strs.size() * (11 + 11 + 3));
        auto iter = strs.cbegin();
        auto end = strs.cend();
        while (iter != end) {
            result += "\x1b[0;36m";
            result += *iter;
            result += "\x1b[0m";
            ++iter;
            if (iter != end) {
                result += " | ";
            }
        }
        return result;
    }

    static constexpr uint32_t Optional = 1;
    static constexpr uint32_t Any = 1 << 1;
    static constexpr uint32_t Array = 1 << 2;
    static constexpr uint32_t ArrayBuffer = 1 << 3;
    static constexpr uint32_t BigInt = 1 << 4;
    static constexpr uint32_t Boolean = 1 << 5;
    static constexpr uint32_t DataView = 1 << 6;
    static constexpr uint32_t Function = 1 << 7;
    static constexpr uint32_t Map = 1 << 8;
    static constexpr uint32_t Null = 1 << 9;
    static constexpr uint32_t Number = 1 << 10;
    static constexpr uint32_t Object = 1 << 11;
    static constexpr uint32_t Promise = 1 << 12;
    static constexpr uint32_t RegExp = 1 << 13;
    static constexpr uint32_t Set = 1 << 14;
    static constexpr uint32_t String = 1 << 15;
    static constexpr uint32_t Symbol = 1 << 16;
    static constexpr uint32_t TypedArray = 1 << 17;
    static constexpr uint32_t Uint8Array = 1 << 18;
    static constexpr uint32_t Undefined = 1 << 19;
};

namespace util {

template<uint32_t... NS>
bool checkFuncArgs(const v8::FunctionCallbackInfo<v8::Value>& info) {
    auto isolate = info.GetIsolate();
    v8::HandleScope handleScope(isolate);
    const auto argNum = info.Length();
    BString expectedName;
    int mismatchIndex = -1;
    int matchCount = 0;
    int optionalCount = 0;
    int argIndex = 0;
    ([&](auto N) {
        if constexpr (N & JS::Optional) {
            ++optionalCount;
            ++matchCount;
        }
        if (argIndex >= argNum) {
            return;
        }
        if constexpr (N & JS::Optional) {
            --matchCount;
        }
        auto value = info[argIndex++];
        if constexpr (N & JS::Any) {
            ++matchCount;
            return;
        }
        if constexpr (N & JS::Array) {
            if (value->IsArray()) {
                ++matchCount;
                return;
            }
        }
        if constexpr (N & JS::ArrayBuffer) {
            if (value->IsArrayBuffer()) {
                ++matchCount;
                return;
            }
        }
        if constexpr (N & JS::BigInt) {
            if (value->IsBigInt()) {
                ++matchCount;
                return;
            }
        }
        if constexpr (N & JS::Boolean) {
            if (value->IsBoolean()) {
                ++matchCount;
                return;
            }
        }
        if constexpr (N & JS::DataView) {
            if (value->IsDataView()) {
                ++matchCount;
                return;
            }
        }
        if constexpr (N & JS::Function) {
            if (value->IsFunction()) {
                ++matchCount;
                return;
            }
        }
        if constexpr (N & JS::Map) {
            if (value->IsMap()) {
                ++matchCount;
                return;
            }
        }
        if constexpr (N & JS::Null) {
            if (value->IsNull()) {
                ++matchCount;
                return;
            }
        }
        if constexpr (N & JS::Number) {
            if (value->IsNumber()) {
                ++matchCount;
                return;
            }
        }
        if constexpr (N & JS::Object) {
            if (!value->IsNull() && value->IsObject()) {
                ++matchCount;
                return;
            }
        }
        if constexpr (N & JS::Promise) {
            if (value->IsPromise()) {
                ++matchCount;
                return;
            }
        }
        if constexpr (N & JS::RegExp) {
            if (value->IsRegExp()) {
                ++matchCount;
                return;
            }
        }
        if constexpr (N & JS::Set) {
            if (value->IsSet()) {
                ++matchCount;
                return;
            }
        }
        if constexpr (N & JS::String) {
            if (value->IsString()) {
                ++matchCount;
                return;
            }
        }
        if constexpr (N & JS::Symbol) {
            if (value->IsSymbol()) {
                ++matchCount;
                return;
            }
        }
        if constexpr (N & JS::TypedArray) {
            if (value->IsTypedArray()) {
                ++matchCount;
                return;
            }
        }
        if constexpr (N & JS::Uint8Array) {
            if (value->IsUint8Array()) {
                ++matchCount;
                return;
            }
        }
        if constexpr (N & JS::Undefined) {
            if (value->IsUndefined()) {
                ++matchCount;
                return;
            }
        }
        mismatchIndex = argIndex;
        expectedName = JS::name<N>();
    }(std::integral_constant<uint32_t, NS>{}), ...);
    constexpr int typeNum = sizeof...(NS);
    if (optionalCount + argNum < typeNum) {
        auto errStr = BString::format(
            "{} argument(s) required, but only {} present",
            typeNum - optionalCount, argNum
        );
        util::throwTypeError(isolate, errStr);
        return false;
    }
    if (matchCount != typeNum) {
        auto errStr = BString::format(
            "Type mismatch at parameter \x1b[0;33m{}\x1b[0m. Expected: {}",
            mismatchIndex, expectedName
        );
        util::throwTypeError(isolate, errStr);
        return false;
    }
    return true;
}

template<uint32_t... NS>
void callAsyncFunc(const v8::FunctionCallbackInfo<v8::Value>& info, AsyncRequest&& req) {
    auto isolate = info.GetIsolate();
    v8::HandleScope handleScope(isolate);
    if (!checkFuncArgs<NS...>(info)) {
        return;
    }
    const auto argNum = info.Length();
    int argIndex = 0;
    int index = 0;
    ([&](auto N) {
        static_assert(
            (N & JS::Boolean) ||
            (N & JS::Number) ||
            (N & JS::String) ||
            (N & JS::ArrayBuffer) ||
            (N & JS::DataView) ||
            (N & JS::TypedArray) ||
            (N & JS::Uint8Array)
        );
        if (argIndex >= argNum) {
            return;
        }
        auto value = info[argIndex++];
        req.set(index, nullptr);
        if constexpr (N & JS::Boolean) {
            if (value->IsBoolean()) {
                auto b = value->BooleanValue(isolate);
                req.set(index, b);
                ++index;
                return;
            }
        }
        if constexpr (N & JS::Number) {
            if (value->IsNumber()) {
                auto n = value.As<v8::Number>()->Value();
                req.set(index, n);
                ++index;
                return;
            }
        }
        if constexpr (N & JS::String) {
            if (value->IsString()) {
                auto v8Str = value.As<v8::String>();
                auto len = v8Str->Utf8Length(isolate);
                auto buf = new char[len + 1];
                v8Str->WriteUtf8(isolate, buf);
                buf[len] = '\0';
                req.set(index, buf);
                ++index;
                return;
            }
        }
        if constexpr (N & JS::ArrayBuffer) {
            if (value->IsArrayBuffer()) {
                auto arrBuf = value.As<v8::ArrayBuffer>();
                auto data = arrBuf->Data();
                auto nbytes = static_cast<int64_t>(arrBuf->ByteLength());
                req.set(index, data);
                ++index;
                req.set(index, nbytes);
                ++index;
                return;
            }
        }
        bool matched = false;
        if constexpr (N & JS::DataView) {
            if (value->IsDataView()) {
                matched = true;
            }
        }
        if constexpr (N & JS::TypedArray) {
            if (value->IsTypedArray()) {
                matched = true;
            }
        }
        if constexpr (N & JS::Uint8Array) {
            if (value->IsUint8Array()) {
                matched = true;
            }
        }
        if (matched) {
            auto abv = value.As<v8::ArrayBufferView>();
            auto arrBuf = abv->Buffer();
            auto offset = abv->ByteOffset();
            auto data = static_cast<char*>(arrBuf->Data()) + offset;
            auto nbytes = static_cast<int64_t>(abv->ByteLength());
            req.set(index, data);
            ++index;
            req.set(index, nbytes);
            ++index;
            return;
        }
    }(std::integral_constant<uint32_t, NS>{}), ...);
    auto context = isolate->GetCurrentContext();
    auto resolver = v8::Promise::Resolver::New(context).ToLocalChecked();
    auto promise = resolver->GetPromise();
    req.setResolver(isolate, resolver);
    auto env = Environment::from(context);
    auto eventLoop = env->getEventLoop();
    eventLoop->submitAsyncRequest(std::move(req));
    info.GetReturnValue().Set(promise);
}

}

}

#endif
