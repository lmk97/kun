#include "web/console.h"

#include <ctype.h>
#include <stddef.h>

#include <vector>

#include "sys/io.h"
#include "sys/time.h"
#include "util/scope_guard.h"
#include "util/utils.h"
#include "util/v8_utils.h"

#ifdef KUN_PLATFORM_WIN32
#undef ERROR
#endif

KUN_V8_USINGS;

using v8::BigIntObject;
using v8::BooleanObject;
using v8::Date;
using v8::Map;
using v8::Name;
using v8::NumberObject;
using v8::RegExp;
using v8::Set;
using v8::SharedArrayBuffer;
using v8::StackTrace;
using v8::StringObject;
using v8::SymbolObject;
using v8::TypedArray;
using kun::BString;
using kun::web::Console;
using kun::sys::microsecond;
using kun::util::formatException;
using kun::util::formatStackTrace;
using kun::util::fromObject;
using kun::util::inObject;
using kun::util::setFunction;
using kun::util::setToStringTag;
using kun::util::throwTypeError;
using kun::util::toBString;
using kun::util::toV8String;

namespace {

enum class LogLevel {
    LOG = 0,
    INFO,
    WARN,
    ERROR
};

constexpr uint32_t GROUP_SIZE = 2;
constexpr uint32_t INDENT_SIZE = 2;
constexpr uint32_t MAX_LINE_WIDTH = 80;

BString formatValue(Local<Context> context, Local<Value> value, Local<Value> root);

template<typename T>
inline void appendIndents(BString& result, T indents) {
    static_assert(
        std::is_same_v<T, uint32_t> ||
        std::is_same_v<T, size_t>
    );
    for (T i = 0; i < indents; i++) {
        result += " ";
    }
}

bool isVarName(const BString& str) {
    auto p = str.data();
    char c = *p++;
    if (!(c == '$' || c == '_' || isalpha(c))) {
        return false;
    }
    auto end = str.data() + str.length();
    while (p < end) {
        c = *p++;
        if (c == '$' || c == '_' || isalnum(c)) {
            continue;
        }
        return false;
    }
    return true;
}

BString escape(const BString& str) {
    const auto len = str.length();
    auto p = str.data();
    auto end = p + len;
    BString result;
    result.reserve((len << 1) + (len >> 1));
    while (p < end) {
        auto c = *p++;
        if (c >= 32 && c <= 126) {
            if (c != '\'') {
                char s[] = {c};
                result.append(s, 1);
            } else {
                char s[] = {'\\', c};
                result.append(s, 2);
            }
        } else if (c >= 7 && c <= 13) {
            constexpr auto base = "abtnvfr";
            char s[] = {'\\', base[c - 7]};
            result.append(s, 2);
        } else {
            bool escapable = false;
            unsigned char u = c;
            if ((u >> 2) == 0x30 && p < end) {
                u = ((u & 0x03) << 6) | (*p & 0x3f);
                if (u >= 128 && u <= 160) {
                    escapable = true;
                    ++p;
                }
            }
            if (escapable) {
                constexpr auto base = "0123456789abcdef";
                auto i = u >> 4;
                auto j = u - (i << 4);
                char s[] = {'\\', 'x', base[i], base[j]};
                result.append(s, 4);
            } else {
                char s[] = {c};
                result.append(s, 1);
            }
        }
    }
    return result;
}

void appendArray(BString& result, const std::vector<BString>& strs, size_t indents) {
    auto iter = strs.cbegin();
    auto end = strs.cend();
    size_t lineWidth = 0;
    while (iter != end) {
        if (lineWidth == 0) {
            appendIndents(result, indents);
            lineWidth = indents;
        }
        const auto& str = *iter;
        ++iter;
        size_t colorLen = 0;
        if (str.startsWith("\x1b[") && str.endsWith("\x1b[0m")) {
            auto index = str.find("m", 3);
            if (index != BString::END) {
                colorLen = index + 5;
            }
        }
        if (colorLen == 0) {
            if (lineWidth > indents) {
                result += "\n";
                appendIndents(result, indents);
            }
            result += str;
            if (iter != end) {
                result += ",\n";
            }
            lineWidth = 0;
            continue;
        }
        auto len = str.length() + 2 - colorLen;
        auto width = lineWidth + len;
        if (width <= MAX_LINE_WIDTH) {
            lineWidth = width;
        } else {
            if (lineWidth > indents) {
                result += "\n";
                appendIndents(result, indents);
            }
            lineWidth = indents + len;
        }
        result += str;
        if (iter != end) {
            result += ", ";
        }
    }
}

void appendObject(BString& result, const std::vector<BString>& strs, size_t indents) {
    auto iter = strs.cbegin();
    auto end = strs.cend();
    while (iter != end) {
        const auto& key = *iter;
        ++iter;
        if (iter == end) {
            break;
        }
        const auto& separator = *iter;
        ++iter;
        if (iter == end) {
            break;
        }
        const auto& value = *iter;
        ++iter;
        appendIndents(result, indents);
        result += key;
        result += separator;
        result += value;
        if (iter != end) {
            result += ",\n";
        }
    }
}

inline BString formatUndefined() {
    return "\x1b[0;30mundefined\x1b[0m";
}

inline BString formatNull() {
    return "\x1b[1;37mnull\x1b[0m";
}

inline BString formatCircular() {
    return "\x1b[0;36m[Circular]\x1b[0m";
}

inline BString formatEllipsis() {
    return "{ \x1b[0;36m...\x1b[0m }";
}

inline BString formatEmpty(uint32_t count) {
    return BString::format("\x1b[0;30mempty x {}\x1b[0m", count);
}

BString formatAccessor(Local<Context> context, Local<Object> obj) {
    BString result;
    result.reserve(11 + 12 + 13);
    result += "\x1b[0;36m";
    result += "[Function: ";
    bool hasGetter = false;
    Local<Function> func;
    if (fromObject(context, obj, "get", func)) {
        result += "Getter";
        hasGetter = true;
    }
    if (fromObject(context, obj, "set", func)) {
        if (hasGetter) {
            result += "/";
        }
        result += "Setter";
    }
    result += "]";
    result += "\x1b[0m";
    return result;
}

template<typename T>
BString formatBigInt(Local<Context> context, T t) {
    static_assert(
        std::is_same_v<T, Local<BigInt>> ||
        std::is_same_v<T, Local<BigIntObject>>
    );
    BString result;
    BString prefix;
    BString suffix;
    Local<String> v8Str;
    if constexpr (std::is_same_v<T, Local<BigInt>>) {
        if (!t->ToString(context).ToLocal(&v8Str)) {
            return result;
        }
    } else {
        if (!t->ValueOf()->ToString(context).ToLocal(&v8Str)) {
            return result;
        }
        prefix = "[BigInt: ";
        suffix = "]";
    }
    auto isolate = context->GetIsolate();
    auto len = v8Str->Utf8Length(isolate);
    result.reserve(11 + 11 + len);
    result += "\x1b[0;33m";
    result += prefix;
    auto buf = result.data() + result.length();
    v8Str->WriteUtf8(isolate, buf);
    result.resize(result.length() + len);
    result += "n";
    result += suffix;
    result += "\x1b[0m";
    return result;
}

template<typename T>
BString formatBoolean(Local<Context> context, T t) {
    static_assert(
        std::is_same_v<T, Local<Boolean>> ||
        std::is_same_v<T, Local<BooleanObject>>
    );
    BString result;
    result.reserve(11 + 11 + 5);
    result += "\x1b[0;33m";
    if constexpr (std::is_same_v<T, Local<Boolean>>) {
        result += t->Value() ? "true" : "false";
    } else {
        result += "[Boolean: ";
        result += t->ValueOf() ? "true" : "false";
        result += "]";
    }
    result += "\x1b[0m";
    return result;
}

BString formatDate(Local<Context> context, Local<Date> date) {
    auto isolate = context->GetIsolate();
    auto isoStr = date->ToISOString();
    auto len = isoStr->Utf8Length(isolate);
    BString result;
    result.reserve(11 + len);
    result += "\x1b[0;35m";
    auto buf = result.data() + result.length();
    isoStr->WriteUtf8(isolate, buf);
    result.resize(result.length() + len);
    result += "\x1b[0m";
    return result;
}

BString formatFunction(Local<Context> context, Local<Function> func) {
    auto isolate = context->GetIsolate();
    Local<String> v8Str;
    size_t len = 0;
    if (func->GetName()->ToString(context).ToLocal(&v8Str)) {
        len = v8Str->Utf8Length(isolate);
    }
    BString result;
    result.reserve(11 + 26 + (len == 0 ? 11 : len));
    result += "\x1b[0;36m";
    result += "[";
    if (func->IsAsyncFunction()) {
        result += "Async";
    }
    if (func->IsGeneratorFunction()) {
        result += "Generator";
    }
    result += "Function";
    if (len == 0) {
        result += " (anonymous)";
    } else {
        result += ": ";
        auto buf = result.data() + result.length();
        v8Str->WriteUtf8(isolate, buf);
        result.resize(result.length() + len);
    }
    result += "]";
    result += "\x1b[0m";
    return result;
}

BString formatName(Local<Context> context, Local<Name> name) {
    auto str = toBString(context, name);
    if (isVarName(str)) {
        return str;
    }
    auto escStr = escape(str);
    BString result;
    result.reserve(11 + 2 + escStr.length());
    if (name->IsSymbol()) {
        result += "[";
        result += "\x1b[0;34m";
        result += escStr;
        result += "\x1b[0m";
        result += "]";
    } else {
        result += "\x1b[0;32m";
        result += "'";
        result += escStr;
        result += "'";
        result += "\x1b[0m";
    }
    return result;
}

template<typename T>
BString formatNumber(Local<Context> context, T t) {
    static_assert(
        std::is_same_v<T, Local<Number>> ||
        std::is_same_v<T, Local<NumberObject>>
    );
    BString result;
    Local<String> v8Str;
    if (!t->ToString(context).ToLocal(&v8Str)) {
        return result;
    }
    auto isolate = context->GetIsolate();
    auto len = v8Str->Utf8Length(isolate);
    result.reserve(11 + 10 + len);
    BString prefix;
    BString suffix;
    if constexpr (std::is_same_v<T, Local<NumberObject>>) {
        prefix = "[Number: ";
        suffix = "]";
    }
    result += "\x1b[0;33m";
    result += prefix;
    auto buf = result.data() + result.length();
    v8Str->WriteUtf8(isolate, buf);
    result.resize(result.length() + len);
    result += suffix;
    result += "\x1b[0m";
    return result;
}

inline BString formatNumber(uint8_t n) {
    return BString::format("\x1b[0;33m{}\x1b[0m", n);
}

BString formatPromise(Local<Context> context, Local<Promise> promise) {
    BString result;
    result.reserve(11 + 12 + 11);
    result += "Promise { ";
    result += "\x1b[0;36m";
    auto state = promise->State();
    if (state == v8::Promise::kPending) {
        result += "<pending>";
    } else if (state == v8::Promise::kFulfilled) {
        result += "<fulfilled>";
    } else {
        result += "<rejected>";
    }
    result += "\x1b[0m";
    result += " }";
    return result;
}

BString formatRegExp(Local<Context> context, Local<RegExp> regExp) {
    BString result;
    Local<String> v8Str;
    if (!regExp->ToString(context).ToLocal(&v8Str)) {
        return result;
    }
    auto isolate = context->GetIsolate();
    auto len = v8Str->Utf8Length(isolate);
    result.reserve(11 + len);
    result += "\x1b[0;31m";
    auto buf = result.data() + result.length();
    v8Str->WriteUtf8(isolate, buf);
    result.resize(result.length() + len);
    result += "\x1b[0m";
    return result;
}

template<typename T>
BString formatString(Local<Context> context, T t) {
    static_assert(
        std::is_same_v<T, Local<String>> ||
        std::is_same_v<T, Local<StringObject>>
    );
    BString result;
    BString color = "\x1b[0;32m";
    if constexpr (std::is_same_v<T, Local<String>>) {
        auto isolate = context->GetIsolate();
        auto len = t->Utf8Length(isolate);
        result.reserve(11 + len);
        result += color;
        auto buf = result.data() + result.length();
        t->WriteUtf8(isolate, buf);
        result.resize(result.length() + len);
    } else {
        auto str = toBString(context, t);
        auto escStr = escape(str);
        result.reserve(11 + 12 + escStr.length());
        result += color;
        result += "[String: '";
        result += escStr;
        result += "']";
    }
    result += "\x1b[0m";
    return result;
}

inline BString formatString(const BString& str) {
    BString result;
    result.reserve(11 + 2 + str.length());
    result += "\x1b[0;32m";
    result += "'";
    result += str;
    result += "'";
    result += "\x1b[0m";
    return result;
}

template<typename T>
inline BString formatSymbol(Local<Context> context, T t) {
    static_assert(
        std::is_same_v<T, Local<Symbol>> ||
        std::is_same_v<T, Local<SymbolObject>>
    );
    auto str = toBString(context, t);
    auto escStr = escape(str);
    BString result;
    result.reserve(11 + 10 + escStr.length());
    BString prefix;
    BString suffix;
    if constexpr (std::is_same_v<T, Local<SymbolObject>>) {
        prefix = "[Symbol: ";
        suffix = "]";
    }
    result += "\x1b[0;34m";
    result += prefix;
    result += escStr;
    result += suffix;
    result += "\x1b[0m";
    return result;
}

template<typename T>
BString handleCircular(
    Local<Context> context,
    Local<Value> value,
    T parent,
    Local<Value> root
) {
    static_assert(
        std::is_same_v<T, Local<Array>> ||
        std::is_same_v<T, Local<Map>> ||
        std::is_same_v<T, Local<Set>> ||
        std::is_same_v<T, Local<Object>>
    );
    if (value->StrictEquals(parent) || value->StrictEquals(root)) {
        return formatCircular();
    }
    if (value->IsString()) {
        auto str = toBString(context, value);
        auto escStr = escape(str);
        return formatString(escStr);
    }
    Local<Value> newRoot = parent;
    if constexpr (std::is_same_v<T, Local<Array>>) {
        if (root->IsArray()) {
            newRoot = root;
        }
    } else if constexpr (std::is_same_v<T, Local<Map>>) {
        if (root->IsMap()) {
            newRoot = root;
        }
    } else if constexpr (std::is_same_v<T, Local<Set>>) {
        if (root->IsSet()) {
            newRoot = root;
        }
    } else if constexpr (std::is_same_v<T, Local<Object>>) {
        if (!root->IsNull() && root->IsObject()) {
            newRoot = root;
        }
    }
    return formatValue(context, value, newRoot);
}

BString formatArray(Local<Context> context, Local<Array> arr, Local<Value> root) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    auto console = Console::from(context);
    if (console == nullptr) {
        return "[]";
    }
    console->indents += INDENT_SIZE;
    ON_SCOPE_EXIT {
        console->indents -= INDENT_SIZE;
    };
    const auto arrLen = arr->Length();
    std::vector<BString> strs;
    strs.reserve(arrLen);
    size_t capacity = 0;
    uint32_t emptyCount = 0;
    for (uint32_t i = 0; i < arrLen; i++) {
        if (!inObject(context, arr, i)) {
            emptyCount++;
            continue;
        }
        if (emptyCount > 0) {
            auto str = formatEmpty(emptyCount);
            capacity += str.length();
            strs.emplace_back(std::move(str));
            emptyCount = 0;
        }
        Local<Value> value;
        if (fromObject(context, arr, i, value)) {
            auto str = handleCircular(context, value, arr, root);
            capacity += str.length();
            strs.emplace_back(std::move(str));
        }
    }
    const auto indents = console->indents;
    capacity += arrLen << 1;
    capacity += (capacity + MAX_LINE_WIDTH - 1) / MAX_LINE_WIDTH * (indents + 1);
    capacity += 4 + indents - INDENT_SIZE;
    BString result;
    result.reserve(capacity);
    result += "[";
    if (arrLen > 0) {
        result += "\n";
        appendArray(result, strs, indents);
        result += "\n";
        appendIndents(result, indents - INDENT_SIZE);
    }
    result += "]";
    return result;
}

template<typename T>
BString formatArrayBuffer(Local<Context> context, T t) {
    static_assert(
        std::is_same_v<T, Local<ArrayBuffer>> ||
        std::is_same_v<T, Local<SharedArrayBuffer>> ||
        std::is_same_v<T, Local<DataView>>
    );
    auto console = Console::from(context);
    if (console == nullptr) {
        return "[]";
    }
    console->indents += INDENT_SIZE;
    ON_SCOPE_EXIT {
        console->indents -= INDENT_SIZE;
    };
    const auto nbytes = t->ByteLength();
    const uint8_t* data;
    if constexpr (std::is_same_v<T, Local<DataView>>) {
        auto arrBuf = t->Buffer();
        auto offset = t->ByteOffset();
        data = static_cast<uint8_t*>(arrBuf->Data()) + offset;
    } else {
        data = static_cast<uint8_t*>(t->Data());
    }
    std::vector<BString> strs;
    strs.reserve(nbytes);
    size_t capacity = 0;
    auto p = data;
    auto end = data + nbytes;
    while (p < end) {
        auto str = formatNumber(*p);
        capacity += str.length();
        strs.emplace_back(std::move(str));
        ++p;
    }
    BString prefix;
    if constexpr (std::is_same_v<T, Local<ArrayBuffer>>) {
        prefix = BString::format("ArrayBuffer({}) ", nbytes);
    } else if constexpr (std::is_same_v<T, SharedArrayBuffer>) {
        prefix = BString::format("SharedArrayBuffer({}) ", nbytes);
    } else {
        prefix = BString::format("DataView({}) ", nbytes);
    }
    const auto indents = console->indents;
    capacity += nbytes << 1;
    capacity += (capacity + MAX_LINE_WIDTH - 1) / MAX_LINE_WIDTH * (indents + 1);
    capacity += prefix.length() + 4 + indents - INDENT_SIZE;
    BString result;
    result.reserve(capacity);
    result += prefix;
    result += "[";
    if (nbytes > 0) {
        result += "\n";
        appendArray(result, strs, indents);
        result += "\n";
        appendIndents(result, indents - INDENT_SIZE);
    }
    result += "]";
    return result;
}

BString formatTypedArray(Local<Context> context, Local<TypedArray> arr) {
    auto console = Console::from(context);
    if (console == nullptr) {
        return "[]";
    }
    console->indents += INDENT_SIZE;
    ON_SCOPE_EXIT {
        console->indents -= INDENT_SIZE;
    };
    const auto arrLen = static_cast<uint32_t>(arr->Length());
    std::vector<BString> strs;
    strs.reserve(arrLen);
    size_t capacity = 0;
    for (uint32_t i = 0; i < arrLen; i++) {
        Local<Value> value;
        if (fromObject(context, arr, i, value)) {
            auto str = formatValue(context, value, arr);
            capacity += str.length();
            strs.emplace_back(std::move(str));
        }
    }
    BString prefix;
    auto toStringTag = Symbol::GetToStringTag(context->GetIsolate());
    if (BString str; fromObject(context, arr, toStringTag, str)) {
        prefix = BString::format("{}({}) ", str, arrLen);
    }
    const auto indents = console->indents;
    capacity += arrLen << 1;
    capacity += (capacity + MAX_LINE_WIDTH - 1) / MAX_LINE_WIDTH * (indents + 1);
    capacity += prefix.length() + 4 + indents - INDENT_SIZE;
    BString result;
    result.reserve(capacity);
    result += prefix;
    result += "[";
    if (arrLen > 0) {
        result += "\n";
        appendArray(result, strs, indents);
        result += "\n";
        appendIndents(result, indents - INDENT_SIZE);
    }
    result += "]";
    return result;
}

template<typename T>
BString formatCollection(Local<Context> context, T t, Local<Value> root) {
    static_assert(
        std::is_same_v<T, Local<Map>> ||
        std::is_same_v<T, Local<Set>>
    );
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    auto console = Console::from(context);
    if (console == nullptr) {
        return "{}";
    }
    console->indents += INDENT_SIZE;
    ON_SCOPE_EXIT {
        console->indents -= INDENT_SIZE;
    };
    Local<Function> func;
    if (!fromObject(context, t, Symbol::GetIterator(isolate), func)) {
        return "{}";
    }
    Local<Object> iterator;
    if (
        Local<Value> value;
        func->Call(context, t, 0, nullptr).ToLocal(&value) &&
        !value->IsNull() &&
        value->IsObject()
    ) {
        iterator = value.As<Object>();
    } else {
        return "{}";
    }
    Local<Function> next;
    if (!fromObject(context, iterator, "next", next)) {
        return "{}";
    }
    const auto arrLen = t->Size();
    std::vector<BString> strs;
    strs.reserve(arrLen);
    size_t capacity = 0;
    bool done = false;
    while (!done) {
        Local<Object> iteratorResult;
        if (
            Local<Value> value;
            next->Call(context, iterator, 0, nullptr).ToLocal(&value) &&
            !value->IsNull() &&
            value->IsObject()
        ) {
            iteratorResult = value.As<Object>();
        } else {
            continue;
        }
        if (fromObject(context, iteratorResult, "done", done) && done) {
            break;
        }
        if constexpr (std::is_same_v<T, Local<Map>>) {
            Local<Value> value1;
            Local<Value> value2;
            Local<Array> items;
            if (
                !fromObject(context, iteratorResult, "value", items) ||
                items->Length() < 2 ||
                !fromObject(context, items, 0, value1) ||
                !fromObject(context, items, 1, value2)
            ) {
                continue;
            }
            auto str1 = handleCircular(context, value1, t, root);
            auto str2 = handleCircular(context, value2, t, root);
            capacity += str1.length() + 4 + str2.length();
            strs.emplace_back(std::move(str1));
            strs.emplace_back(" => ");
            strs.emplace_back(std::move(str2));
        } else {
            Local<Value> value;
            if (!fromObject(context, iteratorResult, "value", value)) {
                continue;
            }
            auto str = handleCircular(context, value, t, root);
            capacity += str.length();
            strs.emplace_back(std::move(str));
        }
    }
    const auto indents = console->indents;
    BString prefix;
    if constexpr (std::is_same_v<T, Local<Map>>) {
        prefix = BString::format("Map({}) ", arrLen);
        capacity += arrLen * (indents + 2);
        capacity += prefix.length() + 4 + indents - INDENT_SIZE;
    } else {
        prefix = BString::format("Set({}) ", arrLen);
        capacity += arrLen << 1;
        capacity += (capacity + MAX_LINE_WIDTH - 1) / MAX_LINE_WIDTH * (indents + 1);
        capacity += prefix.length() + 4 + indents - INDENT_SIZE;
    }
    BString result;
    result.reserve(capacity);
    result += prefix;
    result += "{";
    if (arrLen > 0) {
        result += "\n";
        if constexpr (std::is_same_v<T, Local<Map>>) {
            appendObject(result, strs, indents);
        } else {
            appendArray(result, strs, indents);
        }
        result += "\n";
        appendIndents(result, indents - INDENT_SIZE);
    }
    result += "}";
    return result;
}

BString formatIterator(Local<Context> context, Local<Object> obj, Local<Value> root) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    if (!obj->IsMapIterator() && !obj->IsSetIterator()) {
        return "{}";
    }
    auto console = Console::from(context);
    if (console == nullptr) {
        return "{}";
    }
    console->indents += INDENT_SIZE;
    ON_SCOPE_EXIT {
        console->indents -= INDENT_SIZE;
    };
    Local<Function> next;
    if (!fromObject(context, obj, "next", next)) {
        return "{}";
    }
    std::vector<BString> strs;
    if (obj->IsMapIterator()) {
        strs.reserve(128 * 3);
    } else {
        strs.reserve(128);
    }
    size_t capacity = 0;
    bool done = false;
    while (!done) {
        Local<Object> iteratorResult;
        if (
            Local<Value> value;
            next->Call(context, obj, 0, nullptr).ToLocal(&value) &&
            !value->IsNull() &&
            value->IsObject()
        ) {
            iteratorResult = value.As<Object>();
        } else {
            continue;
        }
        if (fromObject(context, iteratorResult, "done", done) && done) {
            break;
        }
        if (obj->IsMapIterator()) {
            Local<Array> items;
            Local<Value> value1;
            Local<Value> value2;
            if (
                !fromObject(context, iteratorResult, "value", items) ||
                items->Length() < 2 ||
                !fromObject(context, items, 0, value1) ||
                !fromObject(context, items, 1, value2)
            ) {
                continue;
            }
            auto str1 = handleCircular(context, value1, obj, root);
            auto str2 = handleCircular(context, value2, obj, root);
            capacity += str1.length() + 4 + str2.length();
            strs.emplace_back(std::move(str1));
            strs.emplace_back(" => ");
            strs.emplace_back(std::move(str2));
        } else {
            Local<Value> value;
            if (!fromObject(context, iteratorResult, "value", value)) {
                continue;
            }
            auto str = handleCircular(context, value, obj, root);
            capacity += str.length();
            strs.emplace_back(std::move(str));
        }
    }
    const auto indents = console->indents;
    BString prefix;
    size_t arrLen = 0;
    if (obj->IsMapIterator()) {
        prefix = "MapIterator ";
        arrLen = strs.size() / 3;
        capacity += arrLen * (indents + 2);
        capacity += prefix.length() + 4 + indents - INDENT_SIZE;
    } else {
        prefix = "SetIterator ";
        arrLen = strs.size();
        capacity += arrLen << 1;
        capacity += (capacity + MAX_LINE_WIDTH - 1) / MAX_LINE_WIDTH * (indents + 1);
        capacity += prefix.length() + 4 + indents - INDENT_SIZE;
    }
    BString result;
    result.reserve(capacity);
    result += prefix;
    result += "{";
    if (arrLen > 0) {
        result += "\n";
        if (obj->IsMapIterator()) {
            appendObject(result, strs, indents);
        } else {
            appendArray(result, strs, indents);
        }
        result += "\n";
        appendIndents(result, indents - INDENT_SIZE);
    }
    result += "}";
    return result;
}

BString formatObject(Local<Context> context, Local<Object> obj, Local<Value> root) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    auto console = Console::from(context);
    if (console == nullptr) {
        return "{}";
    }
    const auto depth = console->depth;
    if (depth < 0) {
        return formatEllipsis();
    }
    console->depth -= 1;
    console->indents += INDENT_SIZE;
    ON_SCOPE_EXIT {
        console->depth += 1;
        console->indents -= INDENT_SIZE;
    };
    auto propAttr = console->showHidden ? v8::ALL_PROPERTIES : v8::ONLY_ENUMERABLE;
    Local<Array> names;
    if (!obj->GetOwnPropertyNames(context, propAttr).ToLocal(&names)) {
        return "{}";
    }
    const auto namesLen = names->Length();
    std::vector<BString> strs;
    strs.reserve(namesLen * 3);
    size_t capacity = 0;
    for (uint32_t i = 0; i < namesLen; i++) {
        Local<Name> name;
        if (!fromObject(context, names, i, name)) {
            continue;
        }
        BString str;
        Local<Value> value;
        if (
            Local<Value> desc;
            obj->GetOwnPropertyDescriptor(context, name).ToLocal(&desc) &&
            !desc->IsNull() &&
            desc->IsObject()
        ) {
            auto descObj = desc.As<Object>();
            if (
                !inObject(context, descObj, "writable") ||
                !fromObject(context, obj, name, value)
            ) {
                str = formatAccessor(context, descObj);
            }
        } else {
            str = formatUndefined();
        }
        auto str1 = formatName(context, name);
        BString str2;
        if (value.IsEmpty()) {
            str2 = std::move(str);
        } else {
            str2 = handleCircular(context, value, obj, root);
        }
        capacity += str1.length() + 2 + str2.length();
        strs.emplace_back(std::move(str1));
        strs.emplace_back(": ");
        strs.emplace_back(std::move(str2));
    }
    BString prefix;
    auto className = toBString(context, obj->GetConstructorName());
    if (className != "Object") {
        prefix = BString::format("{} ", className);
    }
    const auto indents = console->indents;
    capacity += namesLen * (indents + 2);
    capacity += prefix.length() + 4 + indents - INDENT_SIZE;
    BString result;
    result.reserve(capacity);
    result += prefix;
    result += "{";
    if (namesLen > 0) {
        result += "\n";
        appendObject(result, strs, indents);
        result += "\n";
        appendIndents(result, indents - INDENT_SIZE);
    }
    result += "}";
    return result;
}

BString formatValue(Local<Context> context, Local<Value> value, Local<Value> root) {
    if (value->IsUndefined()) {
        return formatUndefined();
    }
    if (value->IsNull()) {
        return formatNull();
    }
    if (value->IsBigInt()) {
        return formatBigInt(context, value.As<BigInt>());
    }
    if (value->IsBigIntObject()) {
        return formatBigInt(context, value.As<BigIntObject>());
    }
    if (value->IsBoolean()) {
        return formatBoolean(context, value.As<Boolean>());
    }
    if (value->IsBooleanObject()) {
        return formatBoolean(context, value.As<BooleanObject>());
    }
    if (value->IsDate()) {
        return formatDate(context, value.As<Date>());
    }
    if (value->IsFunction()) {
        return formatFunction(context, value.As<Function>());
    }
    if (value->IsNumber()) {
        return formatNumber(context, value.As<Number>());
    }
    if (value->IsNumberObject()) {
        return formatNumber(context, value.As<NumberObject>());
    }
    if (value->IsPromise()) {
        return formatPromise(context, value.As<Promise>());
    }
    if (value->IsRegExp()) {
        return formatRegExp(context, value.As<RegExp>());
    }
    if (value->IsString()) {
        return formatString(context, value.As<String>());
    }
    if (value->IsStringObject()) {
        return formatString(context, value.As<StringObject>());
    }
    if (value->IsSymbol()) {
        return formatSymbol(context, value.As<Symbol>());
    }
    if (value->IsSymbolObject()) {
        return formatSymbol(context, value.As<SymbolObject>());
    }
    if (value->IsArray()) {
        return formatArray(context, value.As<Array>(), root);
    }
    if (value->IsArrayBuffer()) {
        return formatArrayBuffer(context, value.As<ArrayBuffer>());
    }
    if (value->IsSharedArrayBuffer()) {
        return formatArrayBuffer(context, value.As<SharedArrayBuffer>());
    }
    if (value->IsDataView()) {
        return formatArrayBuffer(context, value.As<DataView>());
    }
    if (value->IsTypedArray()) {
        return formatTypedArray(context, value.As<TypedArray>());
    }
    if (value->IsMap()) {
        return formatCollection(context, value.As<Map>(), root);
    }
    if (value->IsSet()) {
        return formatCollection(context, value.As<Set>(), root);
    }
    if (value->IsMapIterator() || value->IsSetIterator()) {
        return formatIterator(context, value.As<Object>(), root);
    }
    if (value->IsNativeError()) {
        return formatException(context, value);
    }
    if (value->IsObject()) {
        return formatObject(context, value.As<Object>(), root);
    }
    return toBString(context, value);
}

size_t findSpecifier(const BString& fmt, size_t offset) {
    auto len = fmt.length();
    auto begin = fmt.data();
    auto end = begin + len;
    auto p = begin + offset;
    while (p < end) {
        char c = *p++;
        if (c != '%') {
            continue;
        }
        c = *p;
        if (
            c == 's' || c == 'd' || c == 'i' ||
            c == 'f' || c == 'o' || c == 'O' ||
            c == 'c' || c == '%'
        ) {
            return p - begin;
        }
    }
    return BString::END;
}

std::vector<BString> format(const FunctionCallbackInfo<Value>& info, int begin, int end) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    std::vector<BString> strs;
    const auto len = info.Length();
    if (begin < 0 || end <= begin || begin >= len) {
        return strs;
    }
    if (end > begin + len) {
        end = begin + len;
    }
    strs.reserve(end - begin);
    auto context = isolate->GetCurrentContext();
    auto console = Console::from(context);
    if (console == nullptr) {
        return strs;
    }
    const auto groupStack = console->groupStack;
    console->indents += groupStack;
    ON_SCOPE_EXIT {
        console->indents -= groupStack;
    };
    if (begin + 1 == end) {
        auto value = info[begin];
        if (value->IsString()) {
            auto str = toBString(context, value);
            strs.emplace_back(std::move(str));
        } else {
            auto str = formatValue(context, value, value);
            strs.emplace_back(std::move(str));
        }
        return strs;
    }
    bool isFirst = true;
    for (auto i = begin; i < end; ) {
        if (isFirst && info[begin]->IsString()) {
            auto globalThis = context->Global();
            Local<Function> parseInt;
            if (!fromObject(context, globalThis, "parseInt", parseInt)) {
                KUN_LOG_ERR("globalThis.parseInt is not found");
                break;
            }
            Local<Function> parseFloat;
            if (!fromObject(context, globalThis, "parseFloat", parseFloat)) {
                KUN_LOG_ERR("globalThis.parseFloat is not found");
                break;
            }
            size_t capacity = 0;
            auto fmt = toBString(context, info[begin]);
            const char* data = fmt.data();
            size_t prev = 0;
            size_t next = 0;
            while ((next = findSpecifier(fmt, next)) != BString::END) {
                auto prevStr = BString::view(data + prev, next - prev - 1);
                capacity += prevStr.length();
                strs.emplace_back(std::move(prevStr));
                auto c = fmt[next];
                prev = ++next;
                if (c == 's') {
                    auto value = info[++i];
                    auto str = toBString(context, value);
                    capacity += str.length();
                    strs.emplace_back(std::move(str));
                } else if (c == 'd' || c == 'i' || c == 'f') {
                    bool isNaN = true;
                    auto value = info[++i];
                    if (!value->IsSymbol()) {
                        auto func = c == 'f' ? parseFloat : parseInt;
                        auto recv = v8::Undefined(isolate);
                        Local<Value> argv[] = {value};
                        Local<Value> result;
                        if (
                            func->Call(context, recv, 1, argv).ToLocal(&result) &&
                            result->IsNumber()
                        ) {
                            auto str = toBString(context, result);
                            capacity += str.length();
                            strs.emplace_back(std::move(str));
                            isNaN = false;
                        }
                    }
                    if (isNaN) {
                        capacity += 3;
                        strs.emplace_back("NaN");
                    }
                } else if (c == 'o' || c == 'O') {
                    auto value = info[++i];
                    auto str = formatValue(context, value, value);
                    capacity += str.length();
                    strs.emplace_back(std::move(str));
                } else if (c == 'c') {
                    // TODO: process %c
                    ++i;
                } else if (c == '%') {
                    capacity += 1;
                    strs.emplace_back("%");
                }
            }
            const auto fmtLen = fmt.length();
            if (prev < fmtLen) {
                auto str = BString::view(data + prev, fmtLen - prev);
                capacity += str.length();
                strs.emplace_back(std::move(str));
            }
            BString str;
            str.reserve(capacity);
            for (const auto& s : strs) {
                str += s;
            }
            strs.clear();
            strs.emplace_back(std::move(str));
            if (++i >= end) {
                break;
            }
            isFirst = false;
        }
        auto value = info[i++];
        if (value->IsString()) {
            auto str = toBString(context, value);
            strs.emplace_back(std::move(str));
        } else {
            auto str = formatValue(context, value, value);
            strs.emplace_back(std::move(str));
        }
    }
    return strs;
}

template<typename... TS>
inline void print(LogLevel logLevel, const BString& fmt, TS&&... args) {
    if (logLevel == LogLevel::ERROR) {
        kun::sys::eprint(fmt, std::forward<TS>(args)...);
    } else {
        kun::sys::print(fmt, std::forward<TS>(args)...);
    }
}

template<typename... TS>
inline void println(LogLevel logLevel, const BString& fmt, TS&&... args) {
    if (logLevel == LogLevel::ERROR) {
        kun::sys::eprintln(fmt, std::forward<TS>(args)...);
    } else {
        kun::sys::println(fmt, std::forward<TS>(args)...);
    }
}

void println(
    LogLevel logLevel,
    const FunctionCallbackInfo<Value>& info,
    int offset = 0,
    int length = -1
) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto context = isolate->GetCurrentContext();
    auto console = Console::from(context);
    if (console == nullptr) {
        return;
    }
    auto indents = console->indents + console->groupStack;
    if (indents > 0) {
        BString str;
        str.reserve(indents);
        appendIndents(str, indents);
        print(logLevel, str);
    }
    if (length == -1) {
        length = info.Length();
    }
    auto strs = format(info, offset, offset + length);
    auto iter = strs.cbegin();
    auto end = strs.cend();
    while (iter != end) {
        print(logLevel, *iter);
        ++iter;
        if (iter != end) {
            print(logLevel, " ");
        }
    }
    println(logLevel, "");
}

void assert(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    const auto len = info.Length();
    constexpr auto logLevel = LogLevel::ERROR;
    if (len <= 0 || !info[0]->BooleanValue(isolate)) {
        print(logLevel, "Assertion failed");
        if (len <= 1) {
            println(logLevel, "");
        } else {
            print(logLevel, ": ");
            println(logLevel, info, 1, len - 1);
        }
    }
}

void clear(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto context = isolate->GetCurrentContext();
    auto console = Console::from(context);
    if (console == nullptr) {
        return;
    }
    console->groupStack = 0;
    print(LogLevel::LOG, "\x1b[2J\x1b[0;0H");
}

void debug(const FunctionCallbackInfo<Value>& info) {
    println(LogLevel::LOG, info);
}

void error(const FunctionCallbackInfo<Value>& info) {
    println(LogLevel::ERROR, info);
}

void info(const FunctionCallbackInfo<Value>& info) {
    println(LogLevel::INFO, info);
}

void log(const FunctionCallbackInfo<Value>& info) {
    println(LogLevel::LOG, info);
}

void table(const FunctionCallbackInfo<Value>& info) {
    println(LogLevel::LOG, info, 0, 1);
}

void trace(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto context = isolate->GetCurrentContext();
    constexpr auto logLevel = LogLevel::LOG;
    print(logLevel, "Trace");
    if (info.Length() <= 0) {
        println(logLevel, "");
    } else {
        print(logLevel, ": ");
        println(logLevel, info);
    }
    auto stackTrace = StackTrace::CurrentStackTrace(isolate, 16);
    auto str = formatStackTrace(context, stackTrace);
    println(logLevel, str);
}

void warn(const FunctionCallbackInfo<Value>& info) {
    println(LogLevel::WARN, info);
}

void dir(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    const auto len = info.Length();
    constexpr auto logLevel = LogLevel::LOG;
    if (len <= 0) {
        auto str = formatUndefined();
        println(logLevel, str);
        return;
    }
    if (info[0]->IsObject()) {
        auto context = isolate->GetCurrentContext();
        auto console = Console::from(context);
        if (console == nullptr) {
            return;
        }
        const auto depth = console->depth;
        const auto showHidden = console->showHidden;
        ON_SCOPE_EXIT {
            console->depth = depth;
            console->showHidden = showHidden;
        };
        if (len > 1 && info[1]->IsObject()) {
            auto options = info[1].As<Object>();
            if (
                Local<Value> value;
                fromObject(context, options, "depth", value)
            ) {
                if (value->IsNumber()) {
                    auto n = value.As<Number>()->Value();
                    console->depth = static_cast<int32_t>(n);
                } else if (value->IsNumberObject()) {
                    auto n = value.As<NumberObject>()->ValueOf();
                    console->depth = static_cast<int32_t>(n);
                } else if (value->IsNull()) {
                    console->depth = 2147483647;
                }
            }
            if (
                bool value = false;
                fromObject(context, options, "showHidden", value)
            ) {
                console->showHidden = value;
            }
        }
        println(logLevel, info, 0, 1);
    } else {
        println(logLevel, info, 0, 1);
    }
}

void dirxml(const FunctionCallbackInfo<Value>& info) {
    println(LogLevel::LOG, info);
}

void count(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto context = isolate->GetCurrentContext();
    auto console = Console::from(context);
    if (console == nullptr) {
        return;
    }
    const auto len = info.Length();
    auto& countMap = console->countMap;
    auto label = len > 0 ? toBString(context, info[0]) : "default";
    auto iter = countMap.find(label);
    auto found = iter != countMap.end();
    uint64_t num = 1;
    if (found) {
        num = ++iter->second;
    }
    println(LogLevel::INFO, "{}: {}", label, num);
    if (!found) {
        countMap.emplace(std::move(label), num);
    }
}

void countReset(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto context = isolate->GetCurrentContext();
    auto console = Console::from(context);
    if (console == nullptr) {
        return;
    }
    const auto len = info.Length();
    auto& countMap = console->countMap;
    auto label = len > 0 ? toBString(context, info[0]) : "default";
    auto iter = countMap.find(label);
    if (iter != countMap.end()) {
        iter->second = 0;
    } else {
        println(LogLevel::WARN, "Count for '{}' does not exist", label);
    }
}

void group(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto context = isolate->GetCurrentContext();
    auto console = Console::from(context);
    if (console == nullptr) {
        return;
    }
    println(LogLevel::LOG, info);
    console->groupStack += GROUP_SIZE;
}

void groupCollapsed(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto context = isolate->GetCurrentContext();
    auto console = Console::from(context);
    if (console == nullptr) {
        return;
    }
    println(LogLevel::LOG, info);
    console->groupStack += GROUP_SIZE;
}

void groupEnd(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto context = isolate->GetCurrentContext();
    auto console = Console::from(context);
    if (console == nullptr) {
        return;
    }
    if (console->groupStack >= GROUP_SIZE) {
        console->groupStack -= GROUP_SIZE;
    }
}

void time(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto context = isolate->GetCurrentContext();
    auto console = Console::from(context);
    if (console == nullptr) {
        return;
    }
    const auto len = info.Length();
    auto& timerTable = console->timerTable;
    auto label = len > 0 ? toBString(context, info[0]) : "default";
    auto iter = timerTable.find(label);
    if (iter == timerTable.end()) {
        auto us = microsecond().unwrap();
        timerTable.emplace(std::move(label), us);
    } else {
        println(LogLevel::LOG, "Timer '{}' already exists", label);
    }
}

void timeLog(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto context = isolate->GetCurrentContext();
    auto console = Console::from(context);
    if (console == nullptr) {
        return;
    }
    const auto len = info.Length();
    constexpr auto logLevel = LogLevel::LOG;
    auto& timerTable = console->timerTable;
    auto label = len > 0 ? toBString(context, info[0]) : "default";
    auto iter = timerTable.find(label);
    if (iter != timerTable.end()) {
        auto us = microsecond().unwrap();
        auto duration = static_cast<double>(us - iter->second) / 1000;
        print(logLevel, "{}: {} ms", label, duration);
        if (len > 1) {
            print(logLevel, " ");
            println(logLevel, info, 1, len - 1);
        } else {
            println(logLevel, "");
        }
    } else {
        println(logLevel, "Timer '{}' does not exists", label);
    }
}

void timeEnd(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto context = isolate->GetCurrentContext();
    auto console = Console::from(context);
    if (console == nullptr) {
        return;
    }
    const auto len = info.Length();
    constexpr auto logLevel = LogLevel::INFO;
    auto& timerTable = console->timerTable;
    auto label = len > 0 ? toBString(context, info[0]) : "default";
    auto iter = timerTable.find(label);
    if (iter != timerTable.end()) {
        auto us = microsecond().unwrap();
        auto duration = static_cast<double>(us - iter->second) / 1000;
        timerTable.erase(iter);
        println(logLevel, "{}: {} ms", label, duration);
    } else {
        println(logLevel, "Timer '{}' does not exists", label);
    }
}

}

namespace kun::web {

Console* Console::from(Local<Context> context) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    auto globalThis = context->Global();
    Local<Object> obj;
    if (fromObject(context, globalThis, "console", obj)) {
        return InternalField<Console>::get(obj, 0);
    }
    throwTypeError(isolate, "Illegal invocation");
    return nullptr;
}

void exposeConsole(Local<Context> context, ExposedScope exposedScope) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    auto objTmpl = ObjectTemplate::New(isolate);
    objTmpl->SetInternalFieldCount(1);
    auto exposedName = toV8String(isolate, "console");
    setToStringTag(isolate, objTmpl, exposedName);
    setFunction(isolate, objTmpl, "assert", assert);
    setFunction(isolate, objTmpl, "clear", clear);
    setFunction(isolate, objTmpl, "debug", debug);
    setFunction(isolate, objTmpl, "error", error);
    setFunction(isolate, objTmpl, "info", info);
    setFunction(isolate, objTmpl, "log", log);
    setFunction(isolate, objTmpl, "table", table);
    setFunction(isolate, objTmpl, "trace", trace);
    setFunction(isolate, objTmpl, "warn", warn);
    setFunction(isolate, objTmpl, "dir", dir);
    setFunction(isolate, objTmpl, "dirxml", dirxml);
    setFunction(isolate, objTmpl, "count", count);
    setFunction(isolate, objTmpl, "countReset", countReset);
    setFunction(isolate, objTmpl, "group", group);
    setFunction(isolate, objTmpl, "groupCollapsed", groupCollapsed);
    setFunction(isolate, objTmpl, "groupEnd", groupEnd);
    setFunction(isolate, objTmpl, "time", time);
    setFunction(isolate, objTmpl, "timeLog", timeLog);
    setFunction(isolate, objTmpl, "timeEnd", timeEnd);
    auto obj = objTmpl->NewInstance(context).ToLocalChecked();
    new Console(obj);
    auto globalThis = context->Global();
    globalThis->DefineOwnProperty(context, exposedName, obj, v8::DontEnum).Check();
}

}
