#ifndef KUN_UTIL_BSTRING_H
#define KUN_UTIL_BSTRING_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <charconv>
#include <system_error>
#include <utility>

#include "util/constants.h"
#include "util/traits.h"

namespace kun {

enum class AcquireBStringView {};

enum class BStringKind {
    STACK = 0x00,
    HEAP = 0x40,
    VIEW = 0x80,
    RODATA = 0xc0
};

class BString {
public:
    BString();

    BString(const char* s, size_t len);

    template<typename T>
    BString(T t);

    template<size_t N>
    BString(const char (&s)[N]);

    BString(const char* s, size_t len, AcquireBStringView a);

    BString(const BString& str);

    BString& operator=(const BString& str);

    BString(BString&& str) noexcept;

    BString& operator=(BString&& str) noexcept;

    ~BString();

    const char* c_str() const;

    const char* data() const;

    char* data();

    size_t length() const;

    size_t capacity() const;

    bool empty() const;

    BString& append(const char* s, size_t len);

    BString& append(const BString& str);

    int compare(const BString& str) const;

    int compareFold(const BString& str) const;

    bool contains(const BString& str) const;

    bool endsWith(const BString& str) const;

    bool equalFold(const BString& str) const;

    size_t find(const BString& str, size_t from = 0) const;

    size_t rfind(const BString& str, size_t from = END) const;

    void reserve(size_t capacity);

    void resize(size_t len);

    bool startsWith(const BString& str) const;

    BString substring(size_t begin, size_t end = END) const;

    template<typename T>
    BString& operator=(T t);

    template<size_t N>
    BString& operator=(const char (&s)[N]);

    BString& operator+=(const BString& str);

    bool operator==(const BString& str) const;

    bool operator!=(const BString& str) const;

    bool operator<(const BString& str) const;

    bool operator<=(const BString& str) const;

    bool operator>(const BString& str) const;

    bool operator>=(const BString& str) const;

    char operator[](size_t index) const;

    char& operator[](size_t index);

    template<typename... TS>
    static BString format(const BString& fmt, TS&&... args);

    static BString view(const char* s, size_t len);

    static BString view(const BString& str);

    static constexpr auto END = static_cast<size_t>(1) << (sizeof(size_t) * 8 - 2);

private:
    BStringKind getKind() const;

    void setStackLength(size_t len);

    void setHeapCapacity(BStringKind kind, size_t capacity);

    static constexpr size_t getStackCapacity();

    union {
        mutable char stack[sizeof(char*) + sizeof(size_t)];
        mutable struct {
            char* data;
            size_t length;
        } heap;
    };
    mutable size_t heapCapacity;
};

inline BString::BString() {
    setStackLength(0);
}

inline BString::BString(const char* s, size_t len) {
    if (len <= getStackCapacity()) {
        memcpy(stack, s, len);
        setStackLength(len);
    } else {
        setStackLength(0);
        append(s, len);
    }
}

template<typename T>
inline BString::BString(T t) : BString(t, strlen(t)) {
    static_assert(kun::is_c_str<T>);
}

template<size_t N>
inline BString::BString(const char (&s)[N]) {
    static_assert(N > 0);
    heap.data = const_cast<char*>(s);
    heap.length = N - 1;
    setHeapCapacity(BStringKind::RODATA, N - 1);
}

inline BString::BString(const char* s, size_t len, AcquireBStringView a) {
    heap.data = const_cast<char*>(s);
    heap.length = len;
    setHeapCapacity(BStringKind::VIEW, len);
}

inline BString::BString(const BString& str) {
    if (str.getKind() == BStringKind::RODATA) {
        heap.data = const_cast<char*>(str.data());
        heap.length = str.length();
        setHeapCapacity(BStringKind::RODATA, str.capacity());
    } else {
        setStackLength(0);
        append(str);
    }
}

inline BString& BString::operator=(const BString& str) {
    if (this == &str) {
        return *this;
    }
    if (str.getKind() == BStringKind::RODATA) {
        if (getKind() == BStringKind::HEAP) {
            free(heap.data);
        }
        heap.data = const_cast<char*>(str.data());
        heap.length = str.length();
        setHeapCapacity(BStringKind::RODATA, str.capacity());
    } else {
        resize(0);
        append(str);
    }
    return *this;
}

inline BString::BString(BString&& str) noexcept {
    if (str.getKind() == BStringKind::STACK) {
        auto len = str.length();
        memcpy(stack, str.data(), len);
        setStackLength(len);
    } else {
        heap.data = str.heap.data;
        heap.length = str.heap.length;
        setHeapCapacity(str.getKind(), str.capacity());
    }
    str.setStackLength(0);
}

inline BString& BString::operator=(BString&& str) noexcept {
    if (this == &str) {
        return *this;
    }
    if (str.getKind() == BStringKind::STACK) {
        resize(0);
        append(str);
    } else {
        if (getKind() == BStringKind::HEAP) {
            free(heap.data);
        }
        heap.data = str.heap.data;
        heap.length = str.heap.length;
        setHeapCapacity(str.getKind(), str.capacity());
    }
    str.setStackLength(0);
    return *this;
}

inline BString::~BString() {
    if (getKind() == BStringKind::HEAP) {
        free(heap.data);
    }
}

inline const char* BString::data() const {
    return getKind() == BStringKind::STACK ? stack : heap.data;
}

inline char* BString::data() {
    return getKind() == BStringKind::STACK ? stack : heap.data;
}

inline size_t BString::length() const {
    if (getKind() == BStringKind::STACK) {
        auto p = reinterpret_cast<uint8_t*>(&heapCapacity);
        auto pn = p + sizeof(heapCapacity) - 1;
        return getStackCapacity() - (*pn & 0x3f);
    } else {
        return heap.length;
    }
}

inline size_t BString::capacity() const {
    if (getKind() == BStringKind::STACK) {
        return getStackCapacity();
    }
    auto cap = heapCapacity;
    auto p = reinterpret_cast<uint8_t*>(&cap);
    auto pn = p + sizeof(cap) - 1;
    if constexpr (KUN_IS_LITTLE_ENDIAN) {
        *pn &= 0x3f;
    } else {
        auto tmp = *p;
        *p = *pn & 0x3f;
        *pn = tmp;
    }
    return cap;
}

inline bool BString::empty() const {
    return length() == 0;
}

inline BString& BString::append(const BString& str) {
    return append(str.data(), str.length());
}

inline bool BString::contains(const BString& str) const {
    return find(str) != END;
}

inline bool BString::endsWith(const BString& str) const {
    return rfind(str) + str.length() == length();
}

inline bool BString::equalFold(const BString& str) const {
    return compareFold(str) == 0;
}

inline void BString::resize(size_t len) {
    if (len > capacity()) {
        return;
    }
    if (getKind() == BStringKind::STACK) {
        setStackLength(len);
    } else {
        heap.length = len;
    }
}

inline bool BString::startsWith(const BString& str) const {
    return find(str) == 0;
}

inline BString BString::substring(size_t begin, size_t end) const {
    auto len = length();
    if (end >= len) {
        end = len;
    }
    return begin < end ? BString::view(data() + begin, end - begin) : "";
}

template<typename T>
inline BString& BString::operator=(T t) {
    static_assert(kun::is_c_str<T>);
    resize(0);
    append(t, strlen(t));
    return *this;
}

template<size_t N>
inline BString& BString::operator=(const char (&s)[N]) {
    static_assert(N > 0);
    if (getKind() == BStringKind::HEAP) {
        free(heap.data);
    }
    heap.data = const_cast<char*>(s);
    heap.length = N - 1;
    setHeapCapacity(BStringKind::RODATA, N - 1);
    return *this;
}

inline BString& BString::operator+=(const BString& str) {
    return append(str);
}

inline bool BString::operator==(const BString& str) const {
    return compare(str) == 0;
}

inline bool BString::operator!=(const BString& str) const {
    return compare(str) != 0;
}

inline bool BString::operator<(const BString& str) const {
    return compare(str) < 0;
}

inline bool BString::operator<=(const BString& str) const {
    return compare(str) <= 0;
}

inline bool BString::operator>(const BString& str) const {
    return compare(str) > 0;
}

inline bool BString::operator>=(const BString& str) const {
    return compare(str) >= 0;
}

inline char BString::operator[](size_t index) const {
    return data()[index];
}

inline char& BString::operator[](size_t index) {
    return data()[index];
}

template<typename... TS>
inline BString BString::format(const BString& fmt, TS&&... args) {
    BString result;
    result.reserve(1023);
    auto p = fmt.data();
    auto end = p + fmt.length();
    auto prev = p;
    ([&](auto&& t) {
        bool found = false;
        while (p < end) {
            if (*p == '{' && p + 1 < end && *(p + 1) == '}') {
                found = true;
                result.append(prev, p - prev);
                p += 2;
                prev = p;
                break;
            }
            p++;
        }
        if (!found) {
            result.append(prev, end - prev);
        }
        using T = typename kun::remove_cvref<decltype(t)>;
        static_assert(
            kun::is_bool<T> ||
            kun::is_char<T> ||
            kun::is_number<T> ||
            kun::is_c_str<T> ||
            std::is_same_v<T, BString>
        );
        if constexpr (kun::is_bool<T>) {
            result += t ? "true" : "false";
        } else if constexpr (kun::is_char<T>) {
            char s[] = {t};
            result.append(s, 1);
        } else if constexpr (kun::is_number<T>) {
            char s[64];
            auto [ptr, ec] = std::to_chars(s, s + sizeof(s), t);
            if (ec == std::errc()) {
                result.append(s, ptr - s);
            }
        } else if constexpr (kun::is_comptime_str<T>) {
            result.append(t, sizeof(T) - 1);
        } else if constexpr (kun::is_c_str<T>) {
            result.append(t, strlen(t));
        } else if constexpr (std::is_same_v<T, BString>) {
            result += t;
        }
    }(std::forward<TS>(args)), ...);
    result.append(prev, end - prev);
    return result;
}

inline BString BString::view(const char* s, size_t len) {
    return BString(s, len, AcquireBStringView());
}

inline BString BString::view(const BString& str) {
    return BString(str.data(), str.length(), AcquireBStringView());
}

inline BStringKind BString::getKind() const {
    auto p = reinterpret_cast<const uint8_t*>(&heapCapacity);
    auto pn = p + sizeof(heapCapacity) - 1;
    return static_cast<BStringKind>(*pn & 0xc0);
}

inline constexpr size_t BString::getStackCapacity() {
    return sizeof(stack) + sizeof(heapCapacity) - 1;
}

inline void BString::setStackLength(size_t len) {
    auto stackCap = getStackCapacity();
    auto kind = static_cast<uint8_t>(BStringKind::STACK);
    auto p = reinterpret_cast<uint8_t*>(&heapCapacity);
    auto pn = p + sizeof(heapCapacity) - 1;
    *pn = (static_cast<uint8_t>(stackCap - len) & 0x3f) | kind;
}

inline void BString::setHeapCapacity(BStringKind kind, size_t capacity) {
    heapCapacity = capacity;
    auto p = reinterpret_cast<uint8_t*>(&heapCapacity);
    auto pn = p + sizeof(heapCapacity) - 1;
    if constexpr (KUN_IS_LITTLE_ENDIAN) {
        *pn |= static_cast<uint8_t>(kind);
    } else {
        auto tmp = *p;
        *p = *pn;
        *pn = tmp | static_cast<uint8_t>(kind);
    }
}

class BStringHash {
public:
    size_t operator()(const BString& str) const {
        size_t h = 0;
        auto p = str.data();
        auto end = p + str.length();
        while (p < end) {
            h = h * 131 + *p++;
        }
        return h;
    }
};

}

#endif
