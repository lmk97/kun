#include "util/bstring.h"

#include <ctype.h>

#include <algorithm>
#include <functional>
#include <string_view>

#include "util/utils.h"

namespace kun {

const char* BString::c_str() const {
    auto s = data();
    auto len = length();
    if (s == nullptr || len == 0) {
        return "";
    }
    if (s[len] == '\0') {
        return s;
    }
    auto kind = getKind();
    if (kind != BStringKind::VIEW && kind != BStringKind::RODATA) {
        auto p = const_cast<char*>(s);
        p[len] = '\0';
        return s;
    }
    auto stackCap = getStackCapacity();
    if (len <= stackCap) {
        memcpy(stack, s, len);
        stack[len] = '\0';
        auto stackKind = static_cast<uint8_t>(BStringKind::STACK);
        auto p = reinterpret_cast<uint8_t*>(&heapCapacity);
        auto pn = p + sizeof(heapCapacity) - 1;
        *pn = static_cast<uint8_t>(stackCap - len) | stackKind;
        return stack;
    } else {
        auto ptr = malloc(len + 1);
        if (ptr == nullptr) {
            KUN_LOG_ERR("'BString::c_str' out of memory");
            ::exit(EXIT_FAILURE);
        }
        auto buf = static_cast<char*>(ptr);
        memcpy(buf, s, len);
        buf[len] = '\0';
        heap.data = buf;
        heap.length = len;
        heapCapacity = len;
        auto p = reinterpret_cast<uint8_t*>(&heapCapacity);
        auto pn = p + sizeof(heapCapacity) - 1;
        if constexpr (KUN_IS_LITTLE_ENDIAN) {
            *pn |= static_cast<uint8_t>(BStringKind::HEAP);
        } else {
            auto tmp = *p;
            *p = *pn;
            *pn = tmp | static_cast<uint8_t>(BStringKind::HEAP);
        }
        return heap.data;
    }
}

BString& BString::append(const char* s, size_t len) {
    if (s == nullptr || len == 0) {
        return *this;
    }
    auto thisLen = length();
    auto thisCap = capacity();
    auto totalLen = thisLen + len;
    auto kind = getKind();
    if (
        kind == BStringKind::VIEW ||
        kind == BStringKind::RODATA ||
        thisCap < totalLen
    ) {
        auto stackCap = getStackCapacity();
        if (totalLen <= stackCap) {
            reserve(stackCap);
        } if (totalLen < 32) {
            reserve(31);
        } else if (totalLen < 48) {
            reserve(47);
        } else if (totalLen < 64) {
            reserve(63);
        } else if (totalLen < 80) {
            reserve(79);
        } else {
            auto cap = thisCap + len;
            reserve(cap + (cap >> 1));
        }
    }
    char* buf = data();
    memcpy(buf + thisLen, s, len);
    if (getKind() == BStringKind::STACK) {
        setStackLength(totalLen);
    } else {
        heap.length = totalLen;
    }
    return *this;
}

int BString::compare(const BString& str) const {
    auto s1 = data();
    auto len1 = length();
    auto s2 = str.data();
    auto len2 = str.length();
    auto len = len1 >= len2 ? len2 : len1;
    int diff = memcmp(s1, s2, len);
    if (diff != 0 || len1 == len2) {
        return diff;
    }
    return len1 > len2 ? 1 : -1;
}

int BString::compareFold(const BString& str) const {
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
        int c1 = *p1++;
        c1 = tolower(c1);
        int c2 = *p2++;
        c2 = tolower(c2);
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

size_t BString::find(const BString& str, size_t from) const {
    auto len = str.length();
    if (len == 0) {
        return 0;
    }
    auto thisLen = length();
    if (thisLen == 0 || from + len > thisLen) {
        return END;
    }
    std::string_view s1(data() + from, thisLen - from);
    std::string_view s2(str.data(), len);
    auto begin = s1.cbegin();
    auto end = s1.cend();
    auto iter = std::search(
        begin,
        end,
        std::boyer_moore_searcher(s2.cbegin(), s2.cend())
    );
    return iter != end ? iter - begin + from : END;
}

size_t BString::rfind(const BString& str, size_t from) const {
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
    std::string_view s1(data(), from + 1);
    std::string_view s2(str.data(), len);
    auto begin = s1.crbegin();
    auto end = s1.crend();
    auto iter = std::search(
        begin,
        end,
        std::boyer_moore_searcher(s2.crbegin(), s2.crend())
    );
    return iter != end ? from - (iter - begin) - len + 1 : END;
}

void BString::reserve(size_t capacity) {
    if (capacity == 0) {
        return;
    }
    auto kind = getKind();
    if (capacity <= getStackCapacity()) {
        if (kind == BStringKind::STACK) {
            return;
        }
        auto s = heap.data;
        auto len = heap.length;
        memcpy(stack, s, len);
        setStackLength(len);
        if (kind == BStringKind::HEAP) {
            free(s);
        }
        return;
    }
    if (kind == BStringKind::HEAP) {
        auto p = realloc(heap.data, capacity + 1);
        if (p == nullptr) {
            KUN_LOG_ERR("'BString::reserve' out of memory");
            ::exit(EXIT_FAILURE);
        }
        heap.data = static_cast<char*>(p);
    } else {
        auto p = malloc(capacity + 1);
        if (p == nullptr) {
            KUN_LOG_ERR("'BString::reserve' out of memory");
            ::exit(EXIT_FAILURE);
        }
        auto buf = static_cast<char*>(p);
        const char* s = data();
        auto len = length();
        memcpy(buf, s, len);
        heap.data = buf;
        heap.length = len;
    }
    setHeapCapacity(BStringKind::HEAP, capacity);
}

}
