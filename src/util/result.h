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

    Result(T&& t) : value(std::move(t)), errCode(0) {
        static_assert(!std::is_same_v<T, SysErr>);
    }

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

    template<typename... TS>
    T&& expect(const BString& fmt, TS&&... args) {
        if (!SysErr::isSuccess(errCode)) {
            auto str = BString::format(fmt, std::forward<TS>(args)...);
            if (!str.empty()) {
                fprintf(stderr, "\x1b[0;31mERROR\x1b[0m: %s\n", str.c_str());
            } else {
                auto [code, phrase] = SysErr(errCode);
                fprintf(
                    stderr,
                    "\x1b[0;31mERROR\x1b[0m: (\x1b[0;33m%d\x1b[0m) %s\n",
                    code, phrase
                );
            }
            ::exit(EXIT_FAILURE);
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
