#ifndef KUN_UTIL_RESULT_H
#define KUN_UTIL_RESULT_H

#include <stdio.h>
#include <stdlib.h>

#include <utility>

#include "util/bstring.h"
#include "util/sys_err.h"

namespace kun {

template<typename T>
class Result {
public:
    Result(const Result&) = delete;

    Result& operator=(const Result&) = delete;

    Result(Result&&) = delete;

    Result& operator=(Result&&) = delete;

    Result(const T& t) : value(t), errCode(0) {}

    Result(T&& t) : value(std::move(t)), errCode(0) {}

    Result(SysErr sysErr) : errCode(sysErr.code) {}

    ~Result() {
        if (SysErr::isSuccess(errCode)) {
            using ValueType = T;
            value.~ValueType();
        }
    }

    operator bool() const {
        return SysErr::isSuccess(errCode);
    }

    SysErr err() const {
        return SysErr(errCode);
    }

    T&& expect(const BString& str) {
        if (!SysErr::isSuccess(errCode)) {
            if (!str.empty()) {
                fprintf(stderr, "\033[0;31mERROR\033[0m: %s\n", str.c_str());
            } else {
                auto [code, phrase] = err();
                fprintf(stderr, "\033[0;31mERROR\033[0m: (%d) %s\n", code, phrase);
            }
            exit(EXIT_FAILURE);
        }
        return std::move(value);
    }

    T&& unwrap() {
        return expect("");
    }

private:
    union {
        T value;
    };
    int errCode;
};

}

#endif
