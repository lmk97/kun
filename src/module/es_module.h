#ifndef KUN_MODULE_ES_MODULE_H
#define KUN_MODULE_ES_MODULE_H

#include <unordered_map>

#include "env/environment.h"
#include "util/bstring.h"
#include "util/result.h"

namespace kun {

class EsModule {
public:
    EsModule(const EsModule&) = delete;

    EsModule& operator=(const EsModule&) = delete;

    EsModule(EsModule&&) = delete;

    EsModule& operator=(EsModule&&) = delete;

    EsModule(Environment* env);

    ~EsModule() = default;

    bool execute(const BString& path);

    bool loadDeps(const BString& path);

    void setModulePath(v8::Local<v8::Module> module, const BString& path);

    void setModulePath(v8::Local<v8::Module> module, BString&& path);

    Result<BString> findModulePath(v8::Local<v8::Module> module) const;

    Result<BString> findDepsPath(const BString& specifier) const;

    Environment* getEnvironment() const {
        return env;
    }

private:
    Environment* env;
    std::unordered_map<int, BString> modulePathMap;
    std::unordered_map<BString, BString, BStringHash> depsPathMap;
};

namespace esm {

v8::MaybeLocal<v8::Promise> importModuleDynamicallyCallback(
    v8::Local<v8::Context> context,
    v8::Local<v8::Data> options,
    v8::Local<v8::Value> resourceName,
    v8::Local<v8::String> specifier,
    v8::Local<v8::FixedArray> importAttrs
);

void importMetaObjectCallback(
    v8::Local<v8::Context> context,
    v8::Local<v8::Module> module,
    v8::Local<v8::Object> meta
);

}

}

#endif
