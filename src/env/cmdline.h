#ifndef KUN_ENV_CMDLINE_H
#define KUN_ENV_CMDLINE_H

#include <unordered_map>
#include <charconv>
#include <system_error>

#include "util/bstring.h"

namespace kun {

class Cmdline {
public:
    Cmdline(const Cmdline&) = delete;

    Cmdline& operator=(const Cmdline&) = delete;

    Cmdline(Cmdline&&) = delete;

    Cmdline& operator=(Cmdline&&) = delete;

    Cmdline(int argc, char** argv);

    template<typename T>
    T get(int optionName) const {
        static_assert(
            std::is_same_v<T, BString> ||
            std::is_integral_v<T> ||
            std::is_floating_point_v<T>
        );
        auto optionValue = getDefaultValue(optionName);
        auto iter = options.find(optionName);
        if (iter != options.end()) {
            optionValue = BString::view(iter->second);
        }
        if constexpr (std::is_same_v<T, BString>) {
            return optionValue;
        } else {
            const char* first = optionValue.data();
            auto last = first + optionValue.length();
            T t{};
            std::from_chars(first, last, t);
            return t;
        }
    }

    BString getProgramPath() const {
        return BString::view(programPath);
    }

    BString getScriptPath() const {
        return BString::view(scriptPath);
    }

    const std::unordered_map<int, BString>& getOptions() const {
        return options;
    }

    const std::unordered_map<BString, BString, BStringHash>& getArguments() const {
        return arguments;
    }

    enum {
        HELP = 0,
        THREAD_POOL_SIZE,
        V8_FLAGS,
        VERSION
    };

private:
    BString getDefaultValue(int optionName) const;

    BString programPath;
    BString scriptPath;
    std::unordered_map<int, BString> options;
    std::unordered_map<BString, BString, BStringHash> arguments;
};

}

#endif
