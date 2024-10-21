#ifndef KUN_UTIL_SYS_ERR_H
#define KUN_UTIL_SYS_ERR_H

#include <stddef.h>

namespace kun {

class SysErr {
public:
    SysErr(int code);

    template<size_t N>
    SysErr(int code, const char (&s)[N]) {
        this->code = code;
        this->phrase = s;
    }

    operator bool() const {
        return SysErr::isSuccess(code);
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

    static bool isSuccess(int code) {
        return code == 0 || code == SUCCESS;
    }

    template<size_t N>
    static SysErr err(const char (&s)[N]) {
        return SysErr(SysErr::RUNTIME_ERROR, s);
    }

    int code;
    const char* phrase;

    enum {
        SUCCESS = 10000,
        UNKNOWN_ERROR,
        RUNTIME_ERROR,
        READ_ERROR,
        WRITE_ERROR,
        INVALID_ARGUMENT,
        INVALID_CHARSET,
        INVALID_SOCKET_TYPE,
        INVALID_SOCKET_VERSION,
        NOT_DIRECTORY,
        NOT_REGULAR_FILE
    };
};

}

#endif
