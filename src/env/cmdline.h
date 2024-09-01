#ifndef KUN_ENV_CMDLINE_H
#define KUN_ENV_CMDLINE_H

#include <charconv>
#include <system_error>
#include <unordered_map>
#include <vector>

#include "util/bstring.h"
#include "util/result.h"
#include "util/sys_err.h"

namespace kun {

class Cmdline {
public:
    Cmdline(const Cmdline&) = delete;

    Cmdline& operator=(const Cmdline&) = delete;

    Cmdline(Cmdline&&) = delete;

    Cmdline& operator=(Cmdline&&) = delete;

    Cmdline(int argc, char** argv);

    ~Cmdline() = default;

    template<typename T>
    Result<T> get(int optionName) const {
        static_assert(
            std::is_same_v<T, BString> ||
            std::is_integral_v<T> ||
            std::is_floating_point_v<T>
        );
        BString optionValue;
        auto iter = options.find(optionName);
        if (iter != options.end()) {
            optionValue = BString::view(iter->second);
        } else {
            if (auto result = getDefaultValue(optionName)) {
                optionValue = result.unwrap();
            } else {
                return result.err();
            }
        }
        if constexpr (std::is_same_v<T, BString>) {
            return optionValue;
        } else {
            const char* first = optionValue.data();
            auto last = first + optionValue.length();
            T t{};
            auto result = std::from_chars(first, last, t);
            if (result.ec == std::errc()) {
                return t;
            }
            return SysErr(SysErr::INVALID_ARGUMENT);
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

    const std::vector<BString>& getArguments() const {
        return arguments;
    }

    enum {
        HELP = 0,
        THREAD_POOL_SIZE,
        V8_FLAGS,
        VERSION
    };

private:
    Result<BString> getDefaultValue(int optionName) const;

    BString programPath;
    BString scriptPath;
    std::unordered_map<int, BString> options;
    std::vector<BString> arguments;
};

}

#endif
