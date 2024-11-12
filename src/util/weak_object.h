#ifndef KUN_UTIL_WEAK_OBJECT_H
#define KUN_UTIL_WEAK_OBJECT_H

#include <stdint.h>

#include "v8.h"

namespace kun {

template<typename T>
class WeakObject {
public:
    WeakObject(const WeakObject&) = delete;

    WeakObject& operator=(const WeakObject&) = delete;

    WeakObject(WeakObject&&) = delete;

    WeakObject& operator=(WeakObject&&) = delete;

    WeakObject(v8::Local<v8::Object> obj, T* t) : data(t) {
        isolate = obj->GetIsolate();
        object.Reset(isolate, obj);
        setWeak();
    }

    ~WeakObject() = default;

    void finalize() {
        object.Reset();
        delete data;
    }

    void ref() {
        if (++refCount == 1) {
            clearWeak();
        }
    }

    void unref() {
        if (refCount > 0 && --refCount == 0) {
            setWeak();
        }
    }

    v8::Local<v8::Object> get() const {
        return object.Get(isolate);
    }

private:
    v8::Isolate* isolate;
    v8::Global<v8::Object> object;
    T* const data;
    uint32_t refCount{0};

    void setWeak() {
        object.SetWeak(
            this,
            [](const v8::WeakCallbackInfo<WeakObject<T>>& info) {
                auto weakObject = info.GetParameter();
                weakObject->finalize();
            },
            v8::WeakCallbackType::kParameter
        );
    }

    void clearWeak() {
        object.ClearWeak();
    }
};

}

#endif
