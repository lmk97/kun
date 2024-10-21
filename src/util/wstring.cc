#include "util/wstring.h"

#include <wctype.h>

#include <algorithm>
#include <functional>
#include <string_view>

#include "util/utils.h"

namespace kun {

const wchar_t* WString::c_str() const {
    auto s = data();
    auto len = length();
    if (s == nullptr || len == 0) {
        return L"";
    }
    if (s[len] == L'\0') {
        return s;
    }
    auto kind = getKind();
    if (kind != WStringKind::VIEW && kind != WStringKind::RODATA) {
        auto p = const_cast<wchar_t*>(s);
        p[len] = L'\0';
        return s;
    }
    auto p = malloc((len + 1) * sizeof(wchar_t));
    if (p == nullptr) {
        KUN_LOG_ERR("'WString::c_str' out of memory");
        ::exit(EXIT_FAILURE);
    }
    auto buf = static_cast<wchar_t*>(p);
    wmemcpy(buf, s, len);
    buf[len] = L'\0';
    wdata = buf;
    wlength = len;
    wcapacity = (len << 2) | static_cast<uint8_t>(WStringKind::HEAP);
    return wdata;
}

WString& WString::append(const wchar_t* s, size_t len) {
    if (s == nullptr || len == 0) {
        return *this;
    }
    auto thisLen = length();
    auto thisCap = capacity();
    auto totalLen = thisLen + len;
    auto kind = getKind();
    if (
        kind == WStringKind::VIEW ||
        kind == WStringKind::RODATA ||
        thisCap < totalLen
    ) {
        auto cap = thisCap + len;
        reserve(cap + (cap >> 1));
    }
    wchar_t* buf = data();
    wmemcpy(buf + thisLen, s, len);
    wlength = totalLen;
    return *this;
}

int WString::compare(const WString& str) const {
    auto s1 = data();
    auto len1 = length();
    auto s2 = str.data();
    auto len2 = str.length();
    auto len = len1 >= len2 ? len2 : len1;
    int diff = wmemcmp(s1, s2, len);
    if (diff != 0 || len1 == len2) {
        return diff;
    }
    return len1 > len2 ? 1 : -1;
}

int WString::compareFold(const WString& str) const {
    auto s1 = data();
    auto len1 = length();
    auto s2 = str.data();
    auto len2 = str.length();
    auto len = len1 >= len2 ? len2 : len1;
    auto p1 = s1;
    auto p2 = s2;
    auto end = s1 + len;
    int diff = 0;
    while (p1 < end) {
        wint_t c1 = *p1++;
        c1 = towlower(c1);
        wint_t c2 = *p2++;
        c2 = towlower(c2);
        if (c1 == c2) {
            continue;
        }
        diff = c1 - c2;
        break;
    }
    if (diff != 0 || len1 == len2) {
        return diff;
    }
    return len1 > len2 ? 1 : -1;
}

size_t WString::find(const WString& str, size_t from) const {
    auto len = str.length();
    if (len == 0) {
        return 0;
    }
    auto thisLen = length();
    if (thisLen == 0 || from + len > thisLen) {
        return END;
    }
    std::wstring_view s1(data() + from, thisLen - from);
    std::wstring_view s2(str.data(), len);
    auto begin = s1.cbegin();
    auto end = s1.cend();
    auto iter = std::search(
        begin,
        end,
        std::boyer_moore_searcher(s2.cbegin(), s2.cend())
    );
    return iter != end ? iter - begin + from : END;
}

size_t WString::rfind(const WString& str, size_t from) const {
    auto len = str.length();
    auto thisLen = length();
    if (len == 0) {
        return thisLen;
    }
    if (thisLen == 0 || from < len) {
        return END;
    }
    if (from == END || from >= thisLen) {
        from = thisLen - 1;
    }
    std::wstring_view s1(data(), from + 1);
    std::wstring_view s2(data(), len);
    auto begin = s1.crbegin();
    auto end = s2.crend();
    auto iter = std::search(
        begin,
        end,
        std::boyer_moore_searcher(s2.crbegin(), s2.crend())
    );
    return iter != end ? from - (iter - begin) - len + 1 : END;
}

void WString::reserve(size_t capacity) {
    if (capacity == 0) {
        return;
    }
    if (getKind() == WStringKind::HEAP) {
        auto p = realloc(wdata, (capacity + 1) * sizeof(wchar_t));
        if (p == nullptr) {
            KUN_LOG_ERR("'WString::reserve' out of memory");
            ::exit(EXIT_FAILURE);
        }
        wdata = static_cast<wchar_t*>(p);
    } else {
        auto p = malloc((capacity + 1) * sizeof(wchar_t));
        if (p == nullptr) {
            KUN_LOG_ERR("'WString::reserve' out of memory");
            ::exit(EXIT_FAILURE);
        }
        auto buf = static_cast<wchar_t*>(p);
        const wchar_t* s = data();
        auto len = length();
        wmemcpy(buf, s, len);
        wdata = buf;
        wlength = len;
    }
    setCapacity(WStringKind::HEAP, capacity);
}

}
