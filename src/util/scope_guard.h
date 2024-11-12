#ifndef KUN_UTIL_SCOPE_GUARD_H
#define KUN_UTIL_SCOPE_GUARD_H

#define KUN_PRIVATE_JOIN_INNER(a, b) a##b

#define KUN_PRIVATE_JOIN(a, b) KUN_PRIVATE_JOIN_INNER(a, b)

#define ON_SCOPE_EXIT \
    auto KUN_PRIVATE_JOIN(Kun_Private_ScopeGuard_, __LINE__) = \
    kun::ScopeGuardSyntaxSupport() + [&]()

namespace kun {

template<typename T>
class ScopeGuard {
public:
    explicit ScopeGuard(T&& t) : func(std::move(t)) {}

    ~ScopeGuard() {
        func();
    }

private:
    T func;
};

class ScopeGuardSyntaxSupport {
public:
    template<typename T>
    ScopeGuard<T> operator+(T&& t) {
        return ScopeGuard<T>(std::forward<T>(t));
    }
};

}

#endif
