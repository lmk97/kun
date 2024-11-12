#ifndef KUN_UTIL_V8_UTILS_H
#define KUN_UTIL_V8_UTILS_H

#include <stddef.h>
#include <stdint.h>

#include <vector>

#include "v8.h"
#include "util/bstring.h"
#include "util/traits.h"

namespace kun {

struct AccessorDescriptor {
    v8::FunctionCallback get{nullptr};
    v8::FunctionCallback set{nullptr};
    bool configurable{true};
    bool enumerable{true};
};

namespace util {

BString toBString(v8::Local<v8::Context> context, v8::Local<v8::Value> value);

BString formatSourceLine(v8::Local<v8::Context> context, v8::Local<v8::Message> message);

BString formatStackTrace(v8::Local<v8::Context> context, v8::Local<v8::StackTrace> stackTrace);

BString formatException(v8::Local<v8::Context> context, v8::Local<v8::Value> exception);

bool instanceOf(
    v8::Local<v8::Context> context,
    v8::Local<v8::Value> value,
    const BString& name
);

void defineAccessor(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> obj,
    const BString& name,
    const AccessorDescriptor& desc
);

void inherit(
    v8::Local<v8::Context> context,
    v8::Local<v8::Function> child,
    const BString& parentName
);

v8::MaybeLocal<v8::Object> createObject(
    v8::Local<v8::Context> context,
    const BString& name,
    int fieldCount
);

inline v8::Local<v8::String> toV8String(v8::Isolate* isolate, const BString& str) {
    return v8::String::NewFromUtf8(
        isolate,
        str.data(),
        v8::NewStringType::kNormal,
        static_cast<int>(str.length())
    ).ToLocalChecked();
}

inline BString formatStackTrace(const BString& path, int line, int column) {
    return BString::format(
        "    at \x1b[0;36m{}\x1b[0m:\x1b[0;33m{}\x1b[0m:\x1b[0;33m{}\x1b[0m",
        path, line, column
    );
}

inline void setToStringTag(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate> objTmpl,
    v8::Local<v8::Value> value
) {
    objTmpl->Set(
        v8::Symbol::GetToStringTag(isolate),
        value,
        static_cast<v8::PropertyAttribute>(v8::ReadOnly | v8::DontEnum)
    );
}

inline void setFunction(
    v8::Isolate* isolate,
    v8::Local<v8::ObjectTemplate> objTmpl,
    const BString& funcName,
    v8::FunctionCallback funcCallback,
    v8::PropertyAttribute propAttr = v8::None
) {
    objTmpl->Set(
        isolate,
        funcName.c_str(),
        v8::FunctionTemplate::New(
            isolate,
            funcCallback,
            v8::Local<v8::Value>(),
            v8::Local<v8::Signature>(),
            0,
            v8::ConstructorBehavior::kThrow
        ),
        propAttr
    );
}

inline void setFunction(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> obj,
    const BString& funcName,
    v8::FunctionCallback funcCallback,
    v8::PropertyAttribute propAttr = v8::None
) {
    obj->DefineOwnProperty(
        context,
        toV8String(context->GetIsolate(), funcName),
        v8::Function::New(
            context,
            funcCallback,
            v8::Local<v8::Value>(),
            0,
            v8::ConstructorBehavior::kThrow
        ).ToLocalChecked(),
        propAttr
    ).Check();
}

inline void throwError(v8::Isolate* isolate, const BString& str) {
    isolate->ThrowException(v8::Exception::Error(toV8String(isolate, str)));
}

inline void throwRangeError(v8::Isolate* isolate, const BString& str) {
    isolate->ThrowException(v8::Exception::RangeError(toV8String(isolate, str)));
}

inline void throwReferenceError(v8::Isolate* isolate, const BString& str) {
    isolate->ThrowException(v8::Exception::ReferenceError(toV8String(isolate, str)));
}

inline void throwSyntaxError(v8::Isolate* isolate, const BString& str) {
    isolate->ThrowException(v8::Exception::SyntaxError(toV8String(isolate, str)));
}

inline void throwTypeError(v8::Isolate* isolate, const BString& str) {
    isolate->ThrowException(v8::Exception::TypeError(toV8String(isolate, str)));
}

template<typename T, typename U>
inline bool fromObject(
    v8::Local<v8::Context> context,
    v8::Local<v8::Object> obj,
    const T& t,
    U& u
) {
    static_assert(
        std::is_same_v<std::remove_cv_t<T>, v8::Local<v8::String>> ||
        std::is_same_v<std::remove_cv_t<T>, v8::Local<v8::Symbol>> ||
        std::is_same_v<std::remove_cv_t<T>, v8::Local<v8::Name>> ||
        std::is_same_v<std::remove_cv_t<T>, v8::Local<v8::Value>> ||
        std::is_same_v<std::remove_cv_t<T>, uint32_t> ||
        std::is_same_v<std::remove_cv_t<T>, BString> ||
        std::is_same_v<std::remove_cv_t<T>, int> ||
        kun::is_c_str<T>
    );
    static_assert(
        std::is_same_v<U, v8::Local<v8::Value>> ||
        std::is_same_v<U, v8::Local<v8::Object>> ||
        std::is_same_v<U, v8::Local<v8::Array>> ||
        std::is_same_v<U, v8::Local<v8::Function>> ||
        std::is_same_v<U, v8::Local<v8::Name>> ||
        std::is_same_v<U, BString> ||
        kun::is_number<U> ||
        kun::is_bool<U>
    );
    auto isolate = context->GetIsolate();
    v8::Local<v8::Value> value;
    if constexpr (std::is_same_v<std::remove_cv_t<T>, BString>) {
        auto v8Str = toV8String(isolate, t);
        if (!obj->Get(context, v8Str).ToLocal(&value)) {
            return false;
        }
    } else if constexpr (kun::is_c_str<T>) {
        auto str = BString::view(t, strlen(t));
        auto v8Str = toV8String(isolate, str);
        if (!obj->Get(context, v8Str).ToLocal(&value)) {
            return false;
        }
    } else {
        if (!obj->Get(context, t).ToLocal(&value)) {
            return false;
        }
    }
    bool success = false;
    if constexpr (std::is_same_v<U, v8::Local<v8::Value>>) {
        u = value;
        success = true;
    } else if constexpr (std::is_same_v<U, v8::Local<v8::Object>>) {
        if (!value->IsNull() && value->IsObject()) {
            u = value.As<v8::Object>();
            success = true;
        }
    } else if constexpr (std::is_same_v<U, v8::Local<v8::Array>>) {
        if (value->IsArray()) {
            u = value.As<v8::Array>();
            success = true;
        }
    } else if constexpr (std::is_same_v<U, v8::Local<v8::Function>>) {
        if (value->IsFunction()) {
            u = value.As<v8::Function>();
            success = true;
        }
    } else if constexpr (std::is_same_v<U, v8::Local<v8::Name>>) {
        if (value->IsName()) {
            u = value.As<v8::Name>();
            success = true;
        }
    } else if constexpr (std::is_same_v<U, BString>) {
        v8::Local<v8::String> v8Str;
        if (value->ToString(context).ToLocal(&v8Str)) {
            u = toBString(context, v8Str);
            success = true;
        }
    } else if constexpr (kun::is_number<U>) {
        if (value->IsNumber()) {
            auto num = value.As<v8::Number>();
            u = static_cast<U>(num->Value());
            success = true;
        }
    } else if constexpr (kun::is_bool<U>) {
        u = value->BooleanValue(isolate);
        success = true;
    }
    return success;
}

template<typename T>
inline bool inObject(v8::Local<v8::Context> context, v8::Local<v8::Object> obj, const T& t) {
    static_assert(
        std::is_same_v<std::remove_cv_t<T>, v8::Local<v8::String>> ||
        std::is_same_v<std::remove_cv_t<T>, v8::Local<v8::Symbol>> ||
        std::is_same_v<std::remove_cv_t<T>, v8::Local<v8::Name>> ||
        std::is_same_v<std::remove_cv_t<T>, v8::Local<v8::Value>> ||
        std::is_same_v<std::remove_cv_t<T>, uint32_t> ||
        std::is_same_v<std::remove_cv_t<T>, BString> ||
        kun::is_c_str<T>
    );
    if constexpr (std::is_same_v<std::remove_cv_t<T>, BString>) {
        auto v8Str = toV8String(context->GetIsolate(), t);
        return obj->Has(context, v8Str).FromMaybe(false);
    } else if constexpr (kun::is_c_str<T>) {
        auto str = BString::view(t, strlen(t));
        auto v8Str = toV8String(context->GetIsolate(), str);
        return obj->Has(context, v8Str).FromMaybe(false);
    } else {
        return obj->Has(context, t).FromMaybe(false);
    }
}

template<typename T>
inline bool fromInternal(v8::Local<v8::Object> obj, int index, T& t) {
    static_assert(
        std::is_same_v<T, v8::Local<v8::Value>> ||
        std::is_same_v<T, v8::Local<v8::External>> ||
        std::is_same_v<T, v8::Local<v8::String>> ||
        std::is_same_v<T, v8::Local<v8::Number>> ||
        std::is_same_v<T, v8::Local<v8::Boolean>> ||
        std::is_same_v<T, BString> ||
        kun::is_number<T> ||
        kun::is_bool<T>
    );
    auto isolate = obj->GetIsolate();
    BString errStr = "Illegal invocation";
    if (obj->InternalFieldCount() <= index) {
        throwTypeError(isolate, errStr);
        return false;
    }
    auto data = obj->GetInternalField(index);
    if (data.IsEmpty() || !data->IsValue()) {
        throwTypeError(isolate, errStr);
        return false;
    }
    auto value = data.As<v8::Value>();
    bool success = false;
    if constexpr (std::is_same_v<T, v8::Local<v8::Value>>) {
        t = value;
        success = true;
    } else if constexpr (std::is_same_v<T, v8::Local<v8::External>>) {
        if (value->IsExternal()) {
            t = value.As<v8::External>();
            success = true;
        }
    } else if constexpr (std::is_same_v<T, v8::Local<v8::String>>) {
        if (value->IsString()) {
            t = value.As<v8::String>();
            success = true;
        }
    } else if constexpr (std::is_same_v<T, v8::Local<v8::Number>>) {
        if (value->IsNumber()) {
            t = value.As<v8::Number>();
            success = true;
        }
    } else if constexpr (std::is_same_v<T, v8::Local<v8::Boolean>>) {
        if (value->IsBoolean()) {
            t = value.As<v8::Boolean>();
            success = true;
        }
    } else if constexpr (std::is_same_v<T, BString>) {
        if (value->IsString()) {
            auto v8Str = value.As<v8::String>();
            auto len = v8Str->Utf8Length(isolate);
            t.resize(0);
            t.reserve(len);
            v8Str->WriteUtf8(isolate, t.data());
            t.resize(len);
            success = true;
        }
    } else if constexpr (kun::is_number<T>) {
        if (value->IsNumber()) {
            auto num = value.As<v8::Number>();
            t = static_cast<T>(num->Value());
            success = true;
        }
    } else if constexpr (kun::is_bool<T>) {
        t = value->BooleanValue(isolate);
        success = true;
    }
    if (!success) {
        throwTypeError(isolate, errStr);
    }
    return success;
}

inline v8::MaybeLocal<v8::Object> getPrototypeOf(
    v8::Local<v8::Context> context,
    v8::Local<v8::Function> func
) {
    auto isolate = context->GetIsolate();
    v8::EscapableHandleScope handleScope(isolate);
    v8::Local<v8::Object> obj;
    if (fromObject(context, func, "prototype", obj)) {
        return handleScope.Escape(obj);
    }
    return v8::MaybeLocal<v8::Object>();
}

template<typename... TS>
v8::MaybeLocal<v8::Object> newInstance(
    v8::Local<v8::Context> context,
    const BString& name,
    TS&&... args
) {
    auto isolate = context->GetIsolate();
    v8::EscapableHandleScope handleScope(isolate);
    auto recv = context->Global();
    size_t prev = 0;
    auto index = name.find(".");
    while (index != BString::END) {
        auto key = name.substring(prev, index);
        prev = index + 1;
        v8::Local<v8::Object> obj;
        if (!fromObject(context, obj, key, obj)) {
            return v8::MaybeLocal<v8::Object>();
        }
        recv = obj;
        index = name.find(".", prev);
    }
    auto className = name.substring(prev);
    std::vector<v8::Local<v8::Value>> values;
    values.reserve(sizeof...(TS));
    ([&](auto&& t) {
        using T = typename kun::remove_cvref<decltype(t)>;
        if constexpr (std::is_same_v<BString, T>) {
            auto v8Str = toV8String(isolate, t);
            values.emplace_back(v8Str);
        } else if constexpr (kun::is_comptime_str<T>) {
            auto str = BString::view(t, sizeof(T) - 1);
            auto v8Str = toV8String(isolate, str);
            values.emplace_back(v8Str);
        } else if constexpr (kun::is_c_str<T>) {
            auto str = BString::view(t, strlen(t));
            auto v8Str = toV8String(isolate, str);
            values.emplace_back(v8Str);
        } else if constexpr (kun::is_bool<T>) {
            auto b = v8::Boolean::New(isolate, t);
            values.emplace_back(b);
        } else if constexpr (kun::is_number<T>) {
            auto num = v8::Number::New(isolate, t);
            values.emplace_back(num);
        } else {
            values.emplace_back(t);
        }
    }(std::forward<TS>(args)), ...);
    v8::Local<v8::Function> constructor;
    if (fromObject(context, recv, className, constructor)) {
        int argc = static_cast<int>(values.size());
        auto argv = values.data();
        v8::Local<v8::Object> obj;
        if (constructor->NewInstance(context, argc, argv).ToLocal(&obj)) {
            return handleScope.Escape(obj);
        }
    }
    return v8::MaybeLocal<v8::Object>();
}

}

}

#endif
