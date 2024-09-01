#include "env/cmdline.h"

#include <stdlib.h>
#include <string.h>

#include "sys/io.h"
#include "sys/path.h"

using kun::BString;
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
    void (*callback)(const BString& value);
};

void printHelp(const BString& value);
void printVersion(const BString& value);

Option OPTIONS[] = {
    {
        "-h", "--help", nullptr,
        "print command line options",
        printHelp
    },
    {
        nullptr, "--thread-pool-size", "4",
        "set the thread pool size",
        nullptr
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

void printHelp(const BString& value) {
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
        str += "\n      ";
        str.append(option.description, strlen(option.description));
        if (option.value != nullptr && strlen(option.value) > 0) {
            str += ". default: ";
            str.append(option.value, strlen(option.value));
        }
        println("{}\n", str);
    }
    ::exit(EXIT_SUCCESS);
}

void printVersion(const BString& value) {
    println("{} {}", KUN_NAME, KUN_VERSION);
    ::exit(EXIT_SUCCESS);
}

int findOption(const BString& name, OptionKind& optionKind) {
    constexpr int n = sizeof(OPTIONS) / sizeof(Option);
    for (int i = 0; i < n; i++) {
        const auto& option = OPTIONS[i];
        BString shortName;
        if (option.shortName != nullptr) {
            shortName = BString::view(
                option.shortName,
                strlen(option.shortName)
            );
        }
        if (name.length() == 2 && name == shortName) {
            optionKind = OptionKind::SHORT_NAME;
            return i;
        }
        auto longName = BString::view(
            option.longName,
            strlen(option.longName)
        );
        if (name == longName) {
            optionKind = OptionKind::LONG_NAME;
            return i;
        }
        if (name.length() > longName.length() + 1 &&
            name.startsWith(longName) &&
            name[longName.length()] == '='
        ) {
            optionKind = OptionKind::LONG_WITH_VALUE;
            return i;
        }
        if (name.length() > 2 &&
            shortName.length() == 2 &&
            name.startsWith(shortName) &&
            !name.contains("=")
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
    options.reserve(argc);
    arguments.reserve(argc);
    bool scriptFound = false;
    for (int i = 1; i < argc; i++) {
        auto name = BString::view(argv[i], strlen(argv[i]));
        if (name.endsWith(".js")) {
            scriptPath = toAbsolutePath(name).unwrap();
            scriptFound = true;
            continue;
        }
        if (!scriptFound) {
            auto optionKind = OptionKind::UNKNOWN;
            auto index = findOption(name, optionKind);
            if (index == -1) {
                println("bad option '{}'", name);
                ::exit(EXIT_FAILURE);
                break;
            }
            const auto& option = OPTIONS[index];
            BString value;
            if (option.value == nullptr) {
                options.emplace(index, "");
            } else {
                if (optionKind == OptionKind::SHORT_WITH_VALUE) {
                    value = name.substring(2);
                } else if (optionKind == OptionKind::LONG_WITH_VALUE) {
                    auto j = name.find("=");
                    value = name.substring(j + 1);
                } else {
                    i++;
                    if (i >= argc) {
                        println("'{}' requires a value", name);
                        ::exit(EXIT_FAILURE);
                        break;
                    }
                    value = BString::view(argv[i], strlen(argv[i]));
                }
                options.emplace(index, BString(value.data(), value.length()));
            }
            if (option.callback != nullptr) {
                option.callback(value);
            }
        } else {
            arguments.emplace_back(name.data(), name.length());
        }
    }
}

Result<BString> Cmdline::getDefaultValue(int optionName) const {
    constexpr int n = sizeof(OPTIONS) / sizeof(Option);
    if (optionName < n) {
        const auto& option = OPTIONS[optionName];
        if (option.value != nullptr) {
            return BString::view(option.value, strlen(option.value));
        }
    }
    return SysErr(SysErr::INVALID_ARGUMENT);
}

}
