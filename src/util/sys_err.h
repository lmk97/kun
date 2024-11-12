#ifndef KUN_UTIL_SYS_ERR_H
#define KUN_UTIL_SYS_ERR_H

#include <stddef.h>

namespace kun {

class SysErr {
public:
    explicit SysErr(int code);

    template<size_t N>
    explicit SysErr(const char (&s)[N]) : code(RUNTIME_ERROR) {
        this->name = "RUNTIME_ERROR";
        this->phrase = s;
    }

    operator bool() const {
        return code == 0;
    }

    bool operator==(int code) const {
        return this->code == code;
    }

    bool operator!=(int code) const {
        return this->code != code;
    }

    bool operator==(const SysErr& sysErr) const {
        return this->code == sysErr.code;
    }

    bool operator!=(const SysErr& sysErr) const {
        return this->code != sysErr.code;
    }

    const int code;
    const char* name;
    const char* phrase;

    enum {
        RUNTIME_ERROR = 10000,
        UNKNOWN_ERROR,
        READ_ERROR,
        WRITE_ERROR,
        NOT_DIRECTORY,
        NOT_REGULAR_FILE,
        INVALID_ARGUMENT,
        INVALID_CHARSET,
        INVALID_SOCKET_TYPE,
        INVALID_SOCKET_VERSION
    };
};

}

#endif
