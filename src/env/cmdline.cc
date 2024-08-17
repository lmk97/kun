#include "env/cmdline.h"

#include <stdlib.h>
#include <string.h>

#include "sys/path.h"

using kun::BString;

namespace {

struct Option {
    const char* shortName;
    const char* longName;
    const char* defaultValue;
    const char* description;
    void (*callback)(const BString& value);
};

void printHelp(const BString& value);
void printVersion(const BString& value);

Option OPTIONS[] = {
    {"-h", "--help", nullptr, "print command line options", printHelp},
    {nullptr, "--thread-pool-size", "4", "set the thread pool size", nullptr},
    {nullptr, "--v8-flags", "", "set the v8 flags", nullptr},
    {"-v", "--version", nullptr, "print " KUN_NAME " version", printVersion}
};

void printHelp(const BString& value) {

}

void printVersion(const BString& value) {

}

}

namespace kun {

Cmdline::Cmdline(int argc, char** argv) {
    auto argv0 = BString::view(argv[0], strlen(argv[0]));
    //programPath = sys::abspath(argv0);


}

BString Cmdline::getDefaultValue(int optionName) const {
    return "";
}

}
