#ifndef KUN_ENV_OPTIONS_H
#define KUN_ENV_OPTIONS_H

#include <unordered_map>
#include <charconv>

#include "util/bstring.h"

namespace kun {

enum class OptionName {
    HELP = 0,
    MAX_CLIENT_SIZE,
    THREAD_POOL_SIZE,
    V8_FLAGS,
    VERSION
};

class Options {
public:
    Options(int argc, char** argv);

    BString getProgramPath() const;

    BString getModulePath() const;

    template<typename T>
    T get(OptionName name) const;

    template<typename T>
    T get(OptionName name, const T& defaultValue) const;

    bool has(OptionName name) const;

private:
    BString programPath;
    BString modulePath;
    std::unordered_map<int, BString> args;
};

inline BString Options::getProgramPath() const {
    return BString::view(programPath);
}

inline BString Options::getModulePath() const {
    return BString::view(modulePath);
}

template<typename T>
inline T Options::get(OptionName name) const {
    static_assert(std::is_same_v<T, int> || std::is_same_v<T, BString>);
    auto it = args.find(static_cast<int>(name));
    if (it == args.end()) {
        return T{};
    }
    if constexpr (std::is_same_v<T, int>) {
        auto& str = it->second;
        const char* first = str.c_str();
        const char* last = first + str.length();
        int value = -1;
        std::from_chars(first, last, value);
        return value;
    }
    if constexpr (std::is_same_v<T, BString>) {
        auto& str = it->second;
        return BString::view(str);
    }
}

template<typename T>
inline T Options::get(OptionName name, const T& defaultValue) const {
    if (has(name)) {
        return get<T>(name);
    }
    return defaultValue;
}

inline bool Options::has(OptionName name) const {
    return args.find(static_cast<int>(name)) != args.end();
}

}

#endif
