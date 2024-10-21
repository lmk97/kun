#ifndef KUN_UTIL_INTERNAL_FIELD_H
#define KUN_UTIL_INTERNAL_FIELD_H

#include "v8.h"
#include "util/v8_utils.h"

namespace kun {

namespace web {

class Console;

}

template<typename T>
class InternalField {
public:
    InternalField(const InternalField&) = delete;

    InternalField& operator=(const InternalField&) = delete;

    InternalField(InternalField&&) = delete;

    InternalField& operator=(InternalField&&) = delete;

    InternalField(T* t) : data(t), type(getType<T>()) {
        static_assert(getType<T>() != -1);
    }

    ~InternalField() = default;

    void set(v8::Local<v8::Object> obj, int index) {
        auto isolate = obj->GetIsolate();
        v8::HandleScope handleScope(isolate);
        auto external = v8::External::New(isolate, this);
        obj->SetInternalField(index, external);
    }

    static T* get(v8::Local<v8::Object> obj, int index) {
        constexpr auto type = getType<T>();
        static_assert(type != -1);
        auto isolate = obj->GetIsolate();
        v8::HandleScope handleScope(isolate);
        v8::Local<v8::External> external;
        if (util::fromInternal(obj, index, external)) {
            auto p = static_cast<InternalField<T>*>(external->Value());
            if (p != nullptr && p->type == type && p->data != nullptr) {
                return p->data;
            }
        }
        util::throwTypeError(isolate, "Illegal invocation");
        return nullptr;
    }

    T* const data;
    const int type;

private:
    template<typename U, int N>
    class TypeValue {
    public:
        static_assert(N >= 0);
        using type = U;
        static constexpr auto value = N;
    };

    template<typename U, typename... US>
    static constexpr int from() {
        if constexpr (std::is_same_v<T, typename U::type>) {
            return U::value;
        } else if constexpr (sizeof...(US) > 0) {
            return InternalField<T>::from<US...>();
        } else {
            return -1;
        }
    }

    enum {
        CONSOLE = 0
    };

    template<typename U>
    static constexpr auto getType = InternalField<U>::template from<
        TypeValue<web::Console, CONSOLE>
    >;
};

}

#endif
