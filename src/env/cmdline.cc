#include "env/cmdline.h"

#include <stdlib.h>
#include <string.h>

#include "sys/io.h"
#include "sys/path.h"

using kun::BString;
using kun::Cmdline;
using kun::sys::eprintln;
using kun::sys::println;
using kun::sys::toAbsolutePath;

namespace {

enum class OptionKind {
    UNKNOWN = 0,
    SHORT_NAME,
    SHORT_WITH_VALUE,
    LONG_NAME,
    LONG_WITH_VALUE
};

struct Option {
    const char* shortName;
    const char* longName;
    const char* value;
    const char* description;
    void (*callback)(int optionName, const BString& optionValue);
};

void printHelp(int optionName, const BString& optionValue);
void printVersion(int optionName, const BString& optionValue);
void checkValue(int optionName, const BString& optionValue);

Option OPTIONS[] = {
    {
        "-h", "--help", nullptr,
        "print command line options",
        printHelp
    },
    {
        nullptr, "--thread-pool-size", "4",
        "set the thread pool size",
        checkValue
    },
    {
        nullptr, "--v8-flags", "",
        "set the v8 flags",
        nullptr
    },
    {
        "-v", "--version", nullptr,
        "print " KUN_NAME " version",
        printVersion
    }
};

void printHelp(int optionName, const BString& optionValue) {
    println("Usage: kun [options] [script.js] [arguments]\n");
    println("Options:");
    constexpr int n = sizeof(OPTIONS) / sizeof(Option);
    for (int i = 0; i < n; i++) {
        BString str;
        str.reserve(255);
        str += "  ";
        const auto& option = OPTIONS[i];
        if (option.shortName != nullptr) {
            str.append(option.shortName, strlen(option.shortName));
            str += ", ";
        }
        str.append(option.longName, strlen(option.longName));
        str += "\n    ";
        str.append(option.description, strlen(option.description));
        if (option.value != nullptr && strlen(option.value) > 0) {
            str += ". default: ";
            str.append(option.value, strlen(option.value));
        }
        println("{}\n", str);
    }
    ::exit(EXIT_SUCCESS);
}

void printVersion(int optionName, const BString& optionValue) {
    println("{} {}", KUN_NAME, KUN_VERSION);
    ::exit(EXIT_SUCCESS);
}

void checkValue(int optionName, const BString& optionValue) {
    constexpr int n = sizeof(OPTIONS) / sizeof(Option);
    if (optionName < 0 || optionName >= n) {
        eprintln("Unknown option name '{}'", optionName);
        return;
    }
    const auto& option = OPTIONS[optionName];
    if (optionName == Cmdline::THREAD_POOL_SIZE) {
        auto first = optionValue.data();
        auto last = first + optionValue.length();
        int value = 0;
        auto result = std::from_chars(first, last, value);
        if (result.ec != std::errc() || value < 2) {
            eprintln("'{}' requires an integer at least 2", option.longName);
            ::exit(EXIT_FAILURE);
        }
    }
}

int findOption(const BString& name, OptionKind& optionKind) {
    constexpr int n = sizeof(OPTIONS) / sizeof(Option);
    for (int i = 0; i < n; i++) {
        const auto& option = OPTIONS[i];
        BString shortName;
        if (option.shortName != nullptr) {
            shortName = BString::view(option.shortName, strlen(option.shortName));
        }
        if (!shortName.empty() && name == shortName) {
            optionKind = OptionKind::SHORT_NAME;
            return i;
        }
        auto longName = BString::view(option.longName, strlen(option.longName));
        if (name == longName) {
            optionKind = OptionKind::LONG_NAME;
            return i;
        }
        if (
            name.length() > longName.length() + 1 &&
            name.startsWith(longName) &&
            name[longName.length()] == '='
        ) {
            optionKind = OptionKind::LONG_WITH_VALUE;
            return i;
        }
        if (
            !shortName.empty() &&
            name.length() > shortName.length() &&
            name.startsWith(shortName)
        ) {
            optionKind = OptionKind::SHORT_WITH_VALUE;
            return i;
        }
    }
    return -1;
}

}

namespace kun {

Cmdline::Cmdline(int argc, char** argv) {
    auto argv0 = BString::view(argv[0], strlen(argv[0]));
    programPath = toAbsolutePath(argv0).unwrap();
    options.reserve(static_cast<size_t>(argc));
    arguments.reserve(static_cast<size_t>(argc));
    bool scriptFound = false;
    for (int i = 1; i < argc; i++) {
        auto name = BString::view(argv[i], strlen(argv[i]));
        if (name.endsWith(".js")) {
            scriptPath = toAbsolutePath(name).unwrap();
            scriptFound = true;
            continue;
        }
        if (scriptFound) {
            arguments.emplace_back(name.data(), name.length());
            continue;
        }
        auto optionKind = OptionKind::UNKNOWN;
        const auto optionName = findOption(name, optionKind);
        if (optionName == -1) {
            println("bad option '{}'", name);
            ::exit(EXIT_FAILURE);
        }
        const auto& option = OPTIONS[optionName];
        BString value;
        if (option.value == nullptr) {
            options.emplace(optionName, "");
        } else {
            if (optionKind == OptionKind::SHORT_WITH_VALUE) {
                auto j = strlen(option.shortName);
                value = name.substring(j);
            } else if (optionKind == OptionKind::LONG_WITH_VALUE) {
                auto j = name.find("=");
                value = name.substring(j + 1);
            } else {
                i++;
                if (i >= argc) {
                    println("'{}' requires a value", name);
                    ::exit(EXIT_FAILURE);
                }
                value = BString::view(argv[i], strlen(argv[i]));
            }
            options.emplace(optionName, BString(value.data(), value.length()));
        }
        if (option.callback != nullptr) {
            option.callback(optionName, value);
        }
    }
}

bool Cmdline::hasValue(int optionName) const {
    constexpr int n = sizeof(OPTIONS) / sizeof(Option);
    if (optionName >= 0 && optionName < n) {
        return OPTIONS[optionName].value != nullptr;
    }
    return false;
}

Result<BString> Cmdline::getDefaultValue(int optionName) const {
    constexpr int n = sizeof(OPTIONS) / sizeof(Option);
    if (optionName >= 0 && optionName < n) {
        const auto& option = OPTIONS[optionName];
        if (option.value != nullptr) {
            return BString::view(option.value, strlen(option.value));
        }
    }
    return SysErr(SysErr::INVALID_ARGUMENT);
}

}
