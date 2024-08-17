#ifndef KUN_LOOP_ASYNC_REQUEST_H
#define KUN_LOOP_ASYNC_REQUEST_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#include "v8.h"
#include "sys/io.h"
#include "util/sys_err.h"
#include "util/bstring.h"
#include "util/v8_util.h"

namespace kun {

class AsyncRequest {
public:
    using HandleFunc = void (*)(AsyncRequest* req);
    using ResolveFunc = void (*)(v8::Local<v8::Context> context, AsyncRequest* req);

    AsyncRequest(const AsyncRequest&) = delete;

    AsyncRequest& operator=(const AsyncRequest&) = delete;

    AsyncRequest(AsyncRequest&&) = default;

    AsyncRequest& operator=(AsyncRequest&&) = default;

    AsyncRequest(HandleFunc handleFunc, ResolveFunc resolveFunc) :
        handleFunc(handleFunc), resolveFunc(resolveFunc) {}

    ~AsyncRequest() = default;

    template<typename T>
    T get(size_t index) const {
        if (index + 1 >= (sizeof(data) >> 3)) {
            sys::eprintln("ERROR: 'AsyncRequest.get' index exceeded");
            return T{};
        }
        if constexpr (std::is_pointer_v<T>) {
            uintptr_t value = 0;
            memcpy(&value, data + (index << 3), sizeof(value));
            return value != 0 ? reinterpret_cast<T>(value) : nullptr;
        } else {
            T value{};
            memcpy(&value, data + (index << 3), sizeof(value));
            return value;
        }
    }

    template<typename T>
    void set(size_t index, T t) {
        if (index + 1 >= (sizeof(data) >> 3)) {
            sys::eprintln("ERROR: 'AsyncRequest.set' index exceeded");
            return;
        }
        if constexpr (std::is_pointer_v<T>) {
            auto value = reinterpret_cast<uintptr_t>(t);
            memcpy(data + (index << 3), &value, sizeof(value));
        } else {
            auto p = reinterpret_cast<char*>(&t);
            memcpy(data + (index << 3), p, sizeof(t));
        }
    }

    v8::Local<v8::Promise::Resolver> getResolver(v8::Isolate* isolate) const {
        return resolver.Get(isolate);
    }

    void setResolver(v8::Isolate* isolate, v8::Local<v8::Promise::Resolver> resolver) {
        this->resolver.Reset(isolate, resolver);
    }

    void handle() {
        handleFunc(this);
    }

    void resolve(v8::Local<v8::Context> context) {
        resolveFunc(context, this);
    }

    template<typename T>
    static void resolveUndefined(v8::Local<v8::Context> context, AsyncRequest* req) {
        static_assert(std::is_integral_v<T> || std::is_same_v<T, void>);
        auto isolate = context->GetIsolate();
        v8::HandleScope handleScope(isolate);
        auto resolver = req->getResolver(isolate);
        if constexpr (std::is_same_v<T, void>) {
            resolver->Resolve(context, v8::Undefined(isolate)).Check();
        } else {
            auto ret = req->get<T>(0);
            if (ret != -1) {
                resolver->Resolve(context, v8::Undefined(isolate)).Check();
            } else {
                auto ec = req->get<int>(1);
                auto [code, phrase] = SysErr(ec);
                auto errStr = BString::format("({}) {}", code, phrase);
                auto errMsg = util::toV8String(isolate, errStr);
                resolver->Reject(context, v8::Exception::TypeError(errMsg)).Check();
            }
        }
    }

    template<typename T>
    static void resolveNumber(v8::Local<v8::Context> context, AsyncRequest* req) {
        static_assert(std::is_integral_v<T> || std::is_floating_point_v<T>);
        auto isolate = context->GetIsolate();
        v8::HandleScope handleScope(isolate);
        auto resolver = req->getResolver(isolate);
        auto ret = req->get<T>(0);
        if (ret != -1) {
            auto n = v8::Number::New(isolate, static_cast<double>(ret));
            resolver->Resolve(context, n).Check();
        } else {
            auto ec = req->get<int>(1);
            auto [code, phrase] = SysErr(ec);
            auto errStr = BString::format("({}) {}", code, phrase);
            auto errMsg = util::toV8String(isolate, errStr);
            resolver->Reject(context, v8::Exception::TypeError(errMsg)).Check();
        }
    }

    static void resolveString(v8::Local<v8::Context> context, AsyncRequest* req) {
        auto isolate = context->GetIsolate();
        v8::HandleScope handleScope(isolate);
        auto resolver = req->getResolver(isolate);
        auto ret = req->get<char*>(0);
        if (ret != nullptr) {
            auto str = v8::String::NewFromUtf8(isolate, ret).ToLocalChecked();
            delete []ret;
            resolver->Resolve(context, str).Check();
        } else {
            auto ec = req->get<int>(1);
            auto [code, phrase] = SysErr(ec);
            auto errStr = BString::format("({}) {}", code, phrase);
            auto errMsg = util::toV8String(isolate, errStr);
            resolver->Reject(context, v8::Exception::TypeError(errMsg)).Check();
        }
    }

    static void resolveUint8Array(v8::Local<v8::Context> context, AsyncRequest* req) {
        auto isolate = context->GetIsolate();
        v8::HandleScope handleScope(isolate);
        auto resolver = req->getResolver(isolate);
        auto ret = req->get<void*>(0);
        if (ret != nullptr) {
            auto nbytes = req->get<size_t>(1);
            auto store = v8::ArrayBuffer::NewBackingStore(
                ret,
                nbytes,
                [](void* data, size_t, void*) -> void {
                    auto buf = static_cast<char*>(data);
                    delete[] buf;
                },
                nullptr
            );
            auto arrBuf = v8::ArrayBuffer::New(isolate, std::move(store));
            auto u8Arr = v8::Uint8Array::New(arrBuf, 0, arrBuf->ByteLength());
            resolver->Resolve(context, u8Arr).Check();
            isolate->AdjustAmountOfExternalAllocatedMemory(nbytes);
        } else {
            auto ec = req->get<int>(1);
            auto [code, phrase] = SysErr(ec);
            auto errStr = BString::format("({}) {}", code, phrase);
            auto errMsg = util::toV8String(isolate, errStr);
            resolver->Reject(context, v8::Exception::TypeError(errMsg)).Check();
        }
    }

private:
    HandleFunc handleFunc;
    ResolveFunc resolveFunc;
    v8::Global<v8::Promise::Resolver> resolver;
    char data[64];
};

}

#endif
