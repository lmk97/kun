#ifndef KUN_UTIL_WSTRING_H
#define KUN_UTIL_WSTRING_H

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <wchar.h>

#include <string>

#include "util/traits.h"

namespace kun {

enum class AcquireWStringView {};

enum class WStringKind {
    HEAP = 1,
    VIEW = 2,
    RODATA = 3
};

class WString {
public:
    WString();

    WString(const wchar_t* s, size_t len);

    template<typename T>
    WString(T t);

    template<size_t N>
    WString(const wchar_t (&s)[N]);

    WString(const wchar_t* s, size_t len, AcquireWStringView a);

    WString(const WString& str);

    WString& operator=(const WString& str);

    WString(WString&&) noexcept;

    WString& operator=(WString&&) noexcept;

    ~WString();

    const wchar_t* c_str() const;

    const wchar_t* data() const;

    wchar_t* data();

    size_t length() const;

    size_t capacity() const;

    bool empty() const;

    WString& append(const wchar_t* s, size_t len);

    WString& append(const WString& str);

    int compare(const WString& str) const;

    int compareFold(const WString& str) const;

    bool contains(const WString& str) const;

    bool endsWith(const WString& str) const;

    bool equalFold(const WString& str) const;

    size_t find(const WString& str, size_t from = 0) const;

    size_t rfind(const WString& str, size_t from = END) const;

    void reserve(size_t capacity);

    void resize(size_t len);

    bool startsWith(const WString& str) const;

    WString substring(size_t begin, size_t end = END) const;

    template<typename T>
    WString& operator=(T t);

    template<size_t N>
    WString& operator=(const wchar_t (&s)[N]);

    WString& operator+=(const WString& str);

    bool operator==(const WString& str) const;

    bool operator!=(const WString& str) const;

    bool operator<(const WString& str) const;

    bool operator<=(const WString& str) const;

    bool operator>(const WString& str) const;

    bool operator>=(const WString& str) const;

    wchar_t operator[](size_t index) const;

    wchar_t& operator[](size_t index);

    template<typename... TS>
    static WString format(const WString& fmt, TS&&... args);

    static WString view(const wchar_t* s, size_t len);

    static WString view(const WString& str);

    static constexpr auto END = static_cast<size_t>(1) << (sizeof(size_t) * 8 - 2);

private:
    WStringKind getKind() const;

    void setCapacity(WStringKind kind, size_t capacity);

    mutable wchar_t* wdata{nullptr};
    mutable size_t wlength{0};
    mutable size_t wcapacity;
};

inline WString::WString() {
    setCapacity(WStringKind::HEAP, 0);
}

inline WString::WString(const wchar_t* s, size_t len) {
    setCapacity(WStringKind::HEAP, 0);
    reserve(len);
    append(s, len);
}

template<typename T>
inline WString::WString(T t) : WString(t, wcslen(t)) {
    static_assert(kun::is_wc_str<T>);
}

template<size_t N>
inline WString::WString(const wchar_t (&s)[N]) {
    static_assert(N > 0);
    wdata = const_cast<wchar_t*>(s);
    wlength = N - 1;
    setCapacity(WStringKind::RODATA, N - 1);
}

inline WString::WString(const wchar_t* s, size_t len, AcquireWStringView a) {
    wdata = const_cast<wchar_t*>(s);
    wlength = len;
    setCapacity(WStringKind::VIEW, len);
}

inline WString::WString(const WString& str) {
    if (str.getKind() == WStringKind::RODATA) {
        wdata = const_cast<wchar_t*>(str.data());
        wlength = str.length();
        setCapacity(WStringKind::RODATA, str.capacity());
    } else {
        setCapacity(WStringKind::HEAP, 0);
        reserve(str.length());
        append(str);
    }
}

inline WString& WString::operator=(const WString& str) {
    if (this == &str) {
        return *this;
    }
    if (str.getKind() == WStringKind::RODATA) {
        if (getKind() == WStringKind::HEAP && wdata != nullptr) {
            free(wdata);
        }
        wdata = const_cast<wchar_t*>(str.data());
        wlength = str.length();
        setCapacity(WStringKind::RODATA, str.capacity());
    } else {
        resize(0);
        append(str);
    }
    return *this;
}

inline WString::WString(WString&& str) noexcept {
    wdata = str.wdata;
    wlength = str.wlength;
    wcapacity = str.wcapacity;
    str.wdata = nullptr;
    str.wlength = 0;
    str.setCapacity(WStringKind::HEAP, 0);
}

inline WString& WString::operator=(WString&& str) noexcept {
    if (this == &str) {
        return *this;
    }
    if (getKind() == WStringKind::HEAP && wdata != nullptr) {
        free(wdata);
    }
    wdata = str.wdata;
    wlength = str.wlength;
    wcapacity = str.wcapacity;
    str.wdata = nullptr;
    str.wlength = 0;
    str.setCapacity(WStringKind::HEAP, 0);
    return *this;
}

inline WString::~WString() {
    if (getKind() == WStringKind::HEAP && wdata != nullptr) {
        free(wdata);
    }
}

inline const wchar_t* WString::data() const {
    return wdata;
}

inline wchar_t* WString::data() {
    return wdata;
}

inline size_t WString::length() const {
    return wlength;
}

inline size_t WString::capacity() const {
    return wcapacity >> 2;
}

inline bool WString::empty() const {
    return length() == 0;
}

inline WString& WString::append(const WString& str) {
    return append(str.data(), str.length());
}

inline bool WString::contains(const WString& str) const {
    return find(str) != END;
}

inline bool WString::endsWith(const WString& str) const {
    return rfind(str) + str.length() == length();
}

inline bool WString::equalFold(const WString& str) const {
    return compareFold(str) == 0;
}

inline void WString::resize(size_t len) {
    if (len > capacity()) {
        return;
    }
    wlength = len;
}

inline bool WString::startsWith(const WString& str) const {
    return find(str) == 0;
}

inline WString WString::substring(size_t begin, size_t end) const {
    auto len = length();
    if (end >= len) {
        end = len;
    }
    return begin < end ? WString::view(data() + begin, end - begin) : L"";
}

template<typename T>
inline WString& WString::operator=(T t) {
    static_assert(kun::is_wc_str<T>);
    resize(0);
    append(t, wcslen(t));
    return *this;
}

template<size_t N>
inline WString& WString::operator=(const wchar_t (&s)[N]) {
    static_assert(N > 0);
    if (getKind() == WStringKind::HEAP && wdata != nullptr) {
        free(wdata);
    }
    wdata = const_cast<wchar_t*>(s);
    wlength = N - 1;
    setCapacity(WStringKind::RODATA, N - 1);
    return *this;
}

inline WString& WString::operator+=(const WString& str) {
    return append(str);
}

inline bool WString::operator==(const WString& str) const {
    return compare(str) == 0;
}

inline bool WString::operator!=(const WString& str) const {
    return compare(str) != 0;
}

inline bool WString::operator<(const WString& str) const {
    return compare(str) < 0;
}

inline bool WString::operator<=(const WString& str) const {
    return compare(str) <= 0;
}

inline bool WString::operator>(const WString& str) const {
    return compare(str) > 0;
}

inline bool WString::operator>=(const WString& str) const {
    return compare(str) >= 0;
}

inline wchar_t WString::operator[](size_t index) const {
    return data()[index];
}

inline wchar_t& WString::operator[](size_t index) {
    return data()[index];
}

template<typename... TS>
inline WString WString::format(const WString& fmt, TS&&... args) {
    WString result;
    result.reserve(1023);
    auto p = fmt.data();
    auto end = p + fmt.length();
    auto prev = p;
    ([&](auto&& t) {
        bool found = false;
        while (p < end) {
            if (*p == L'{' && p + 1 < end && *(p + 1) == L'}') {
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
            kun::is_wchar<T> ||
            kun::is_number<T> ||
            kun::is_wc_str<T> ||
            std::is_same_v<T, WString>
        );
        if constexpr (kun::is_bool<T>) {
            result += t ? L"true" : L"false";
        } else if constexpr (kun::is_wchar<T>) {
            wchar_t s[] = {t};
            result.append(s, 1);
        } else if constexpr (kun::is_number<T>) {
            auto s = std::to_wstring(t);
            result.append(s.data(), s.length());
        } else if constexpr (kun::is_comptime_wstr<T>) {
            result.append(t, sizeof(T) - 1);
        } else if constexpr (kun::is_wc_str<T>) {
            result.append(t, wcslen(t));
        } else if constexpr (std::is_same_v<T, WString>) {
            result += t;
        }
    }(std::forward<TS>(args)), ...);
    result.append(prev, end - prev);
    return result;
}

inline WString WString::view(const wchar_t* s, size_t len) {
    return WString(s, len, AcquireWStringView());
}

inline WString WString::view(const WString& str) {
    return WString(str.data(), str.length(), AcquireWStringView());
}

inline WStringKind WString::getKind() const {
    return static_cast<WStringKind>(wcapacity & 0x3);
}

inline void WString::setCapacity(WStringKind kind, size_t capacity) {
    wcapacity = (capacity << 2) | static_cast<uint8_t>(kind);
}

class WStringHash {
public:
    size_t operator()(const WString& str) const {
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
