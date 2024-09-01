#ifndef KUN_UTIL_CONSTANT_H
#define KUN_UTIL_CONSTANT_H

#define KUN_NAME "Kun"

#define KUN_VERSION "0.0.1"

#ifdef _MSC_VER
#ifdef KUN_BIG_ENDIAN
#define KUN_IS_LITTLE_ENDIAN false
#else
#define KUN_IS_LITTLE_ENDIAN true
#endif
#else
#define KUN_IS_LITTLE_ENDIAN (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__)
#endif

#if defined(__linux__)
#define KUN_PLATFORM_LINUX
#elif defined(__APPLE__)
#define KUN_PLATFORM_DARWIN
#elif defined(_WIN32)
#define KUN_PLATFORM_WIN32
#endif

#if defined(KUN_PLATFORM_LINUX) || defined(KUN_PLATFORM_DARWIN)
#define KUN_PLATFORM_UNIX
#endif

#if defined(KUN_PLATFORM_UNIX)
#define KUN_SYS kun::unix
#define KUN_PATH_SEPARATOR "/"
#elif defined(KUN_PLATFORM_WIN32)
#define KUN_SYS kun::win
#define KUN_PATH_SEPARATOR "\\"
#endif

#define KUN_V8_USINGS \
    using v8::Array; \
    using v8::ArrayBuffer; \
    using v8::Boolean; \
    using v8::Context; \
    using v8::DataView; \
    using v8::EscapableHandleScope; \
    using v8::Function; \
    using v8::FunctionCallbackInfo; \
    using v8::FunctionTemplate; \
    using v8::Global; \
    using v8::HandleScope; \
    using v8::Isolate; \
    using v8::Local; \
    using v8::MaybeLocal; \
    using v8::Number; \
    using v8::Object; \
    using v8::ObjectTemplate; \
    using v8::Promise; \
    using v8::PropertyAttribute; \
    using v8::String; \
    using v8::Symbol; \
    using v8::Uint8Array; \
    using v8::Value

namespace kun {

enum class ExposedScope {
    MAIN = 0,
    WORKER
};

}

#endif
