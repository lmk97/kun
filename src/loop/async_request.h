#ifndef KUN_LOOP_ASYNC_REQUEST_H
#define KUN_LOOP_ASYNC_REQUEST_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "v8.h"
#include "util/bstring.h"
#include "util/scope_guard.h"
#include "util/sys_err.h"
#include "util/traits.h"
#include "util/utils.h"
#include "util/v8_utils.h"

namespace kun {

class AsyncRequest {
public:
    using HandleFunc = void (*)(AsyncRequest& req);
    using ResolveFunc = void (*)(v8::Local<v8::Context> context, AsyncRequest& req);

    AsyncRequest(const AsyncRequest&) = delete;

    AsyncRequest& operator=(const AsyncRequest&) = delete;

    AsyncRequest(AsyncRequest&& req) noexcept :
        handleFunc(req.handleFunc),
        resolveFunc(req.resolveFunc),
        resolver(std::move(req.resolver))
    {
        memcpy(data, req.data, sizeof(data));
        req.handleFunc = nullptr;
        req.resolveFunc = nullptr;
    }

    AsyncRequest& operator=(AsyncRequest&& req) noexcept {
        handleFunc = req.handleFunc;
        resolveFunc = req.resolveFunc;
        resolver = std::move(req.resolver);
        memcpy(data, req.data, sizeof(data));
        req.handleFunc = nullptr;
        req.resolveFunc = nullptr;
        return *this;
    }

    AsyncRequest(HandleFunc handleFunc, ResolveFunc resolveFunc) :
        handleFunc(handleFunc),
        resolveFunc(resolveFunc)
    {

    }

    ~AsyncRequest() = default;

    template<typename T>
    T get(size_t index) const {
        if (index + 1 >= (sizeof(data) >> 3)) {
            KUN_LOG_ERR("'AsyncRequest::get' index exceeded");
            ::exit(EXIT_FAILURE);
        }
        if constexpr (std::is_pointer_v<T>) {
            uintptr_t value = 0;
            memcpy(&value, data + (index << 3), sizeof(value));
            return value != 0 ? reinterpret_cast<T>(value) : nullptr;
        } else if constexpr (kun::is_bool<T>) {
            int value{};
            memcpy(&value, data + (index << 3), sizeof(value));
            return value == 0 ? false : true;
        } else {
            T value{};
            memcpy(&value, data + (index << 3), sizeof(value));
            return value;
        }
    }

    template<typename T>
    void set(size_t index, T t) {
        if (index + 1 >= (sizeof(data) >> 3)) {
            KUN_LOG_ERR("'AsyncRequest::set' index exceeded");
            ::exit(EXIT_FAILURE);
        }
        if constexpr (std::is_pointer_v<T>) {
            uintptr_t value = 0;
            if (t != nullptr) {
                value = reinterpret_cast<uintptr_t>(t);
            }
            memcpy(data + (index << 3), &value, sizeof(value));
        } else if constexpr (kun::is_bool<T>) {
            int value = t ? 1 : 0;
            memcpy(data + (index << 3), &value, sizeof(value));
        } else {
            memcpy(data + (index << 3), &t, sizeof(t));
        }
    }

    v8::Local<v8::Promise::Resolver> getResolver(v8::Isolate* isolate) const {
        return resolver.Get(isolate);
    }

    void setResolver(v8::Isolate* isolate, v8::Local<v8::Promise::Resolver> resolver) {
        this->resolver.Reset(isolate, resolver);
    }

    void handle() {
        handleFunc(*this);
    }

    void resolve(v8::Local<v8::Context> context) {
        resolveFunc(context, *this);
    }

    template<typename T>
    static void resolveUndefined(v8::Local<v8::Context> context, AsyncRequest& req) {
        static_assert(kun::is_number<T> || std::is_same_v<T, void>);
        auto isolate = context->GetIsolate();
        v8::HandleScope handleScope(isolate);
        auto resolver = req.getResolver(isolate);
        if constexpr (std::is_same_v<T, void>) {
            resolver->Resolve(context, v8::Undefined(isolate)).Check();
        } else {
            auto ret = req.get<T>(0);
            if (ret != -1) {
                resolver->Resolve(context, v8::Undefined(isolate)).Check();
            } else {
                auto errCode = req.get<int>(1);
                auto [code, phrase] = SysErr(errCode);
                auto errStr = BString::format("({}) {}", code, phrase);
                auto v8Str = util::toV8String(isolate, errStr);
                resolver->Reject(context, v8::Exception::TypeError(v8Str)).Check();
            }
        }
    }

    template<typename T>
    static void resolveNumber(v8::Local<v8::Context> context, AsyncRequest& req) {
        static_assert(kun::is_number<T>);
        auto isolate = context->GetIsolate();
        v8::HandleScope handleScope(isolate);
        auto resolver = req.getResolver(isolate);
        auto ret = req.get<T>(0);
        if (ret != -1) {
            auto num = v8::Number::New(isolate, ret);
            resolver->Resolve(context, num).Check();
        } else {
            auto errCode = req.get<int>(1);
            auto [code, phrase] = SysErr(errCode);
            auto errStr = BString::format("({}) {}", code, phrase);
            auto v8Str = util::toV8String(isolate, errStr);
            resolver->Reject(context, v8::Exception::TypeError(v8Str)).Check();
        }
    }

    static void resolveString(v8::Local<v8::Context> context, AsyncRequest& req) {
        auto isolate = context->GetIsolate();
        v8::HandleScope handleScope(isolate);
        auto resolver = req.getResolver(isolate);
        auto ret = req.get<char*>(0);
        if (ret != nullptr) {
            ON_SCOPE_EXIT {
                delete[] ret;
            };
            auto len = req.get<size_t>(1);
            auto str = BString::view(ret, len);
            auto v8Str = util::toV8String(isolate, str);
            resolver->Resolve(context, v8Str).Check();
        } else {
            auto errCode = req.get<int>(1);
            if (errCode == 0) {
                resolver->Resolve(context, v8::Null(isolate)).Check();
                return;
            }
            auto [code, phrase] = SysErr(errCode);
            auto errStr = BString::format("({}) {}", code, phrase);
            auto v8Str = util::toV8String(isolate, errStr);
            resolver->Reject(context, v8::Exception::TypeError(v8Str)).Check();
        }
    }

    static void resolveUint8Array(v8::Local<v8::Context> context, AsyncRequest& req) {
        auto isolate = context->GetIsolate();
        v8::HandleScope handleScope(isolate);
        auto resolver = req.getResolver(isolate);
        auto ret = req.get<void*>(0);
        if (ret != nullptr) {
            auto len = req.get<size_t>(1);
            auto store = v8::ArrayBuffer::NewBackingStore(
                ret,
                len,
                [](void* data, size_t, void*) -> void {
                    auto buf = static_cast<char*>(data);
                    delete[] buf;
                },
                nullptr
            );
            isolate->AdjustAmountOfExternalAllocatedMemory(static_cast<int64_t>(len));
            auto arrBuf = v8::ArrayBuffer::New(isolate, std::move(store));
            auto u8Arr = v8::Uint8Array::New(arrBuf, 0, arrBuf->ByteLength());
            resolver->Resolve(context, u8Arr).Check();
        } else {
            auto errCode = req.get<int>(1);
            if (errCode == 0) {
                resolver->Resolve(context, v8::Null(isolate)).Check();
                return;
            }
            auto [code, phrase] = SysErr(errCode);
            auto errStr = BString::format("({}) {}", code, phrase);
            auto v8Str = util::toV8String(isolate, errStr);
            resolver->Reject(context, v8::Exception::TypeError(v8Str)).Check();
        }
    }

private:
    HandleFunc handleFunc;
    ResolveFunc resolveFunc;
    v8::Global<v8::Promise::Resolver> resolver;
    char data[48];
};

}

#endif
