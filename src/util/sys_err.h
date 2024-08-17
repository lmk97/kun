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

    bool operator==(int code) {
        return this->code == code;
    }

    bool operator!=(int code) {
        return this->code != code;
    }

    bool operator==(const SysErr& sysErr) {
        return code == sysErr.code;
    }

    bool operator!=(const SysErr& sysErr) {
        return code != sysErr.code;
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
        UNKNOWN = SUCCESS,
        RUNTIME_ERROR,
        INVALID_ARGUMENT,
        INVALID_CHARSET,
        INVALID_SOCKET_TYPE,
        INVALID_SOCKET_VERSION
    };
};

}

#endif