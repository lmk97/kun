#include "env/options.h"

#include <stdlib.h>

#include "util/constants.h"
#include "util/util.h"
#include "sys/io.h"

using kun::BString;
using kun::util::toAbsolutePath;
using kun::sys::println;

namespace {

struct OptionConfig {
    const char* longName;
    const char* shortName;
    const char* description;
    bool requireValue;
    void (*callback)(const BString& value);
};

void printHelp(const BString& value);
void printVersion(const BString& value);

OptionConfig OPTION_CONFIGS[] = {
    {
        "--help",
        "-h",
        "print command line options",
        false,
        printHelp
    },
    {
        "--max-client-size",
        "",
        "set max client size for event loop",
        true,
        nullptr
    },
    {
        "--thread-pool-size",
        "",
        "set thread pool size for async io",
        true,
        nullptr
    },
    {
        "--v8-flags",
        "",
        "set v8 flags, e.g. --v8-flags=\"--help\"",
        true,
        nullptr
    },
    {
        "--version",
        "-v",
        "print version",
        false,
        printVersion
    }
};

void printHelp(const BString& value) {
    BString result;
    result.reserve(2047);
    result += "Usage: kun [options] [script.js] [arguments]\n\nOptions:\n";
    for (int i = 0, l = sizeof(OPTION_CONFIGS) / sizeof(OptionConfig); i < l; i++) {
        auto& cfg = OPTION_CONFIGS[i];
        result += "  ";
        auto shortName = cfg.shortName;
        auto len = strlen(shortName);
        if (len > 0) {
            result.append(shortName, len);
            result += ", ";
        }
        result.append(cfg.longName, strlen(cfg.longName));
        result += "\n";
        result += "      ";
        result.append(cfg.description, strlen(cfg.description));
        result += "\n\n";
    }
    kun::sys::print(result);
}

void printVersion(const BString& value) {
    println("Kun {}", KUN_VERSION);
}

int findOptionConfig(const BString& name) {
    for (int i = 0, l = sizeof(OPTION_CONFIGS) / sizeof(OptionConfig); i < l; i++) {
        auto& cfg = OPTION_CONFIGS[i];
        auto longName = BString::view(cfg.longName, strlen(cfg.longName));
        if (name.startsWith(longName)) {
            if (name.length() == longName.length() || name[longName.length()] == '=') {
                return i;
            }
        }
        auto shortName = BString::view(cfg.shortName, strlen(cfg.shortName));
        if (!shortName.empty() && name.startsWith(shortName)) {
            return i;
        }
    }
    return -1;
}

BString parseOptionValue(const BString& value) {
    auto len = value.length();
    auto hasQuote = value[0] == '\'' || value[0] == '"';
    if (hasQuote && value[0] != value[len - 1]) {
        println("bad option value: {}", value);
        return "";
    }
    BString str;
    if (hasQuote) {
        str += value.substring(1, len - 1);
    } else {
        str += value;
    }
    return str;
}

}

namespace kun {

Options::Options(int argc, char** argv) {
    auto argv0 = BString::view(argv[0], strlen(argv[0]));
    programPath = toAbsolutePath(argv0);
    args.reserve(argc);
    for (int i = 1; i < argc; i++) {
        auto name = BString::view(argv[i], strlen(argv[i]));
        if (name[0] == '-') {
            int index = findOptionConfig(name);
            if (index == -1) {
                println("bad option: {}", name);
                exit(EXIT_FAILURE);
                break;
            }
            auto& cfg = OPTION_CONFIGS[index];
            if (cfg.requireValue) {
                BString str;
                if (name.startsWith("--")) {
                    auto j = name.find("=");
                    if (j != BString::END) {
                        str = name.substring(j + 1);
                    } else if (i + 1 < argc) {
                        const char* p = argv[++i];
                        str = BString::view(p, strlen(p));
                    }
                } else {
                    if (name.length() > 2) {
                        str = name.substring(2);
                    } else if (i + 1 < argc) {
                        const char* p = argv[++i];
                        str = BString::view(p, strlen(p));
                    }
                }
                if (str.empty()) {
                    println("{} requires a value", name);
                    exit(EXIT_FAILURE);
                    break;
                }
                auto value = parseOptionValue(str);
                if (cfg.callback != nullptr) {
                    cfg.callback(value);
                    exit(0);
                }
                args.emplace(index, std::move(value));
            } else {
                if (cfg.callback != nullptr) {
                    cfg.callback("");
                    exit(0);
                }
                args.emplace(index, "");
            }
        } else if (modulePath.empty()) {
            if (name.endsWith(".js")) {
                modulePath = toAbsolutePath(name);
            }
        }
    }
}

}
