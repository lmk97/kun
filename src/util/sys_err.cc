#include "util/sys_err.h"

#include <string.h>

namespace {

const char* SYS_ERR_PHRASES[] = {
    "Success",
    "Unknown error",
    "Runtime error",
    "Invalid argument",
    "Invalid Unicode character",
    "Socket type not supported",
    "Socket version not supported"
};

}

namespace kun {

SysErr::SysErr(int code) {
    if (code < SUCCESS) {
        this->code = code;
        this->phrase = strerror(code);
    } else {
        this->code = code;
        int index = code - SUCCESS;
        int len = sizeof(SYS_ERR_PHRASES) / sizeof(const char*);
        if (index < len) {
            this->phrase = SYS_ERR_PHRASES[index];
        } else {
            this->phrase = SYS_ERR_PHRASES[UNKNOWN - SUCCESS];
        }
    }
}

}
