#include "module/es_module.h"

#include "v8.h"
#include "sys/fs.h"
#include "sys/io.h"
#include "sys/path.h"
#include "util/constants.h"
#include "util/scope_guard.h"
#include "util/utils.h"
#include "util/v8_utils.h"

KUN_V8_USINGS;

using v8::Data;
using v8::Exception;
using v8::FixedArray;
using v8::JSON;
using v8::Location;
using v8::MemorySpan;
using v8::Message;
using v8::Module;
using v8::ModuleRequest;
using v8::ScriptCompiler;
using v8::ScriptOrigin;
using v8::StackTrace;
using v8::TryCatch;
using kun::BString;
using kun::Environment;
using kun::EsModule;
using kun::sys::cleanPath;
using kun::sys::dirname;
using kun::sys::eprintln;
using kun::sys::isAbsolutePath;
using kun::sys::joinPath;
using kun::sys::pathExists;
using kun::sys::readFile;
using kun::util::formatException;
using kun::util::formatSourceLine;
using kun::util::formatStackTrace;
using kun::util::fromObject;
using kun::util::throwError;
using kun::util::throwSyntaxError;
using kun::util::toBString;
using kun::util::toV8String;

namespace {

class DynamicModuleData {
public:
    DynamicModuleData(const DynamicModuleData&) = delete;

    DynamicModuleData& operator=(const DynamicModuleData&) = delete;

    DynamicModuleData(DynamicModuleData&&) = default;

    DynamicModuleData& operator=(DynamicModuleData&&) = default;

    DynamicModuleData() = default;

    ~DynamicModuleData() = default;

    Isolate* isolate;
    Global<Context> context;
    Global<Data> options;
    Global<Value> resourceName;
    Global<String> specifier;
    Global<FixedArray> importAttrs;
    Global<Promise::Resolver> resolver;
    Global<Value> exception;
};

BString findAttrType(Local<Context> context, Local<FixedArray> importAttrs, bool hasOffset) {
    int entrySize = hasOffset ? 3 : 2;
    for (int i = 0; i < importAttrs->Length(); i += entrySize) {
        auto key = importAttrs->Get(context, i).As<Value>();
        auto str = toBString(context, key);
        if (str != "type") {
            continue;
        }
        auto value = importAttrs->Get(context, i + 1).As<Value>();
        return toBString(context, value);
    }
    return "";
}

Location findLocation(
    Local<Context> context,
    Local<String> specifier,
    Local<FixedArray> importAttrs,
    Local<Module> referrer
) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    auto attrsLen = importAttrs->Length();
    auto requests = referrer->GetModuleRequests();
    for (int i = 0; i < requests->Length(); i++) {
        auto req = requests->Get(context, i).As<ModuleRequest>();
        auto attrs = req->GetImportAttributes();
        if (
            attrsLen != attrs->Length() ||
            !specifier->StringEquals(req->GetSpecifier())
        ) {
            continue;
        }
        bool found = true;
        for (int j = 0; j < attrsLen; j++) {
            auto value1 = importAttrs->Get(context, j).As<Value>();
            auto value2 = attrs->Get(context, j).As<Value>();
            if (!value1->StrictEquals(value2)) {
                found = false;
                break;
            }
        }
        if (found) {
            auto offset = req->GetSourceOffset();
            return referrer->SourceOffsetToLocation(offset);
        }
    }
    return Location(-1, -1);
}

BString formatMessage(Local<Context> context, Local<Message> message) {
    BString result;
    auto line = message->GetLineNumber(context).FromMaybe(-1);
    auto column = message->GetStartColumn(context).FromMaybe(-1);
    if (line < 1 || column == -1) {
        return result;
    }
    result.reserve(511);
    result += toBString(context, message->Get());
    auto sourceLine = formatSourceLine(context, message);
    if (!sourceLine.empty()) {
        result += "\n";
        result += sourceLine;
    }
    result += "\n";
    auto path = toBString(context, message->GetScriptResourceName());
    result += formatStackTrace(path, line, column + 1);
    return result;
}

BString formatJsonError(Local<Context> context, Local<Value> exception, const BString& path) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    BString result;
    auto message = Exception::CreateMessage(isolate, exception);
    auto line = message->GetLineNumber(context).FromMaybe(-1);
    auto column = message->GetStartColumn(context).FromMaybe(-1);
    if (line < 1 || column == -1) {
        return result;
    }
    result.reserve(511);
    if (exception->IsNativeError()) {
        auto obj = exception.As<Object>();
        BString str;
        if (fromObject(context, obj, "message", str)) {
            result += str;
        } else {
            result += "Malformed JSON";
        }
    } else {
        result += toBString(context, exception);
    }
    auto sourceLine = formatSourceLine(context, message);
    if (!sourceLine.empty()) {
        result += "\n";
        result += sourceLine;
    }
    result += "\n";
    result += formatStackTrace(path, line, column + 1);
    return result;
}

inline bool isLocalPath(const BString& path) {
    return isAbsolutePath(path) || path.startsWith("./") || path.startsWith("../");
}

MaybeLocal<Value> jsonModuleEvaluationSteps(Local<Context> context, Local<Module> module) {
    auto isolate = context->GetIsolate();
    EscapableHandleScope handleScope(isolate);
    auto resolver = Promise::Resolver::New(context).ToLocalChecked();
    auto promise = resolver->GetPromise();
    auto env = Environment::from(context);
    auto esModule = env->getEsModule();
    BString modulePath;
    if (auto result = esModule->findModulePath(module)) {
        modulePath = result.unwrap();
    } else {
        auto v8Str = toV8String(isolate, "JSON not found");
        resolver->Reject(context, Exception::Error(v8Str)).Check();
        return handleScope.Escape(promise);
    }
    auto content = readFile(modulePath).expect("JSON not found '{}'", modulePath);
    TryCatch tryCatch(isolate);
    Local<Value> value;
    if (JSON::Parse(context, toV8String(isolate, content)).ToLocal(&value)) {
        resolver->Resolve(context, v8::Undefined(isolate)).Check();
        return handleScope.Escape(promise);
    }
    if (tryCatch.HasCaught()) {
        auto exception = tryCatch.Exception();
        auto errStr = formatJsonError(context, exception, modulePath);
        auto v8Str = toV8String(isolate, errStr);
        resolver->Reject(context, Exception::SyntaxError(v8Str)).Check();
    } else {
        auto errStr = BString::format("Malformed JSON '{}'", modulePath);
        auto v8Str = toV8String(isolate, errStr);
        resolver->Reject(context, Exception::SyntaxError(v8Str)).Check();
    }
    return handleScope.Escape(promise);
}

MaybeLocal<Module> resolveModuleCallback(
    Local<Context> context,
    Local<String> specifier,
    Local<FixedArray> importAttrs,
    Local<Module> referrer
) {
    auto isolate = context->GetIsolate();
    EscapableHandleScope handleScope(isolate);
    auto env = Environment::from(context);
    auto esModule = env->getEsModule();
    BString referrerPath;
    if (auto result = esModule->findModulePath(referrer)) {
        referrerPath = result.unwrap();
    } else {
        throwError(isolate, "Referrer module not found");
        return MaybeLocal<Module>();
    }
    BString modulePath;
    auto specifierStr = toBString(context, specifier);
    if (isLocalPath(specifierStr)) {
        auto referrerDir = dirname(referrerPath);
        modulePath = joinPath(referrerDir, specifierStr);
    } else {
        if (auto result = esModule->findDepsPath(specifierStr)) {
            modulePath = result.unwrap();
        } else {
            auto location = findLocation(context, specifier, importAttrs, referrer);
            auto line = location.GetLineNumber() + 1;
            auto column = location.GetColumnNumber() + 1;
            auto errStr = BString::format(
                "Dependency not found '{}'\n{}",
                specifierStr, formatStackTrace(referrerPath, line, column)
            );
            throwError(isolate, errStr);
            return MaybeLocal<Module>();
        }
    }
    BString content;
    if (auto result = readFile(modulePath)) {
        content = result.unwrap();
    } else {
        auto location = findLocation(context, specifier, importAttrs, referrer);
        auto line = location.GetLineNumber() + 1;
        auto column = location.GetColumnNumber() + 1;
        auto errStr = BString::format(
            "Module not found '{}'\n{}",
            modulePath, formatStackTrace(referrerPath, line, column)
        );
        throwError(isolate, errStr);
        return MaybeLocal<Module>();
    }
    auto attrType = findAttrType(context, importAttrs, true);
    if (attrType == "json") {
        auto moduleName = toV8String(isolate, modulePath);
        const MemorySpan<const Local<String>> exportNames(
            {toV8String(isolate, "default")}
        );
        auto module = Module::CreateSyntheticModule(
            isolate,
            moduleName,
            exportNames,
            jsonModuleEvaluationSteps
        );
        esModule->setModulePath(module, std::move(modulePath));
        return handleScope.Escape(module);
    }
    ScriptOrigin scriptOrigin(
        isolate,
        toV8String(isolate, modulePath),
        0,
        0,
        false,
        -1,
        Local<Value>(),
        false,
        false,
        true,
        Local<Data>()
    );
    ScriptCompiler::Source source(toV8String(isolate, content), scriptOrigin);
    TryCatch tryCatch(isolate);
    Local<Module> module;
    if (ScriptCompiler::CompileModule(isolate, &source).ToLocal(&module)) {
        esModule->setModulePath(module, std::move(modulePath));
        return handleScope.Escape(module);
    }
    if (!tryCatch.HasCaught()) {
        auto location = findLocation(context, specifier, importAttrs, referrer);
        auto line = location.GetLineNumber() + 1;
        auto column = location.GetColumnNumber() + 1;
        auto errStr = BString::format(
            "Failed to import '{}'\n{}",
            specifierStr, formatStackTrace(referrerPath, line, column)
        );
        throwSyntaxError(isolate, errStr);
    }
    tryCatch.ReThrow();
    return MaybeLocal<Module>();
}

void doImportModuleDynamically(void* ptr) {
    auto data = static_cast<DynamicModuleData*>(ptr);
    ON_SCOPE_EXIT {
        delete data;
    };
    auto isolate = data->isolate;
    auto context = data->context.Get(isolate);
    auto resourceName = data->resourceName.Get(isolate);
    auto specifier = data->specifier.Get(isolate);
    auto importAttrs = data->importAttrs.Get(isolate);
    auto resolver = data->resolver.Get(isolate);
    auto exception = data->exception.Get(isolate);
    auto env = Environment::from(context);
    auto esModule = env->getEsModule();
    BString modulePath;
    auto specifierStr = toBString(context, specifier);
    if (isLocalPath(specifierStr)) {
        auto referrerPath = toBString(context, resourceName);
        auto referrerDir = dirname(referrerPath);
        modulePath = joinPath(referrerDir, specifierStr);
    } else {
        if (auto result = esModule->findDepsPath(specifierStr)) {
            modulePath = result.unwrap();
        } else {
            resolver->Reject(context, exception).Check();
            return;
        }
    }
    BString content;
    if (auto result = readFile(modulePath)) {
        content = result.unwrap();
    } else {
        resolver->Reject(context, exception).Check();
        return;
    }
    auto attrType = findAttrType(context, importAttrs, false);
    if (attrType == "json") {
        TryCatch tryCatch(isolate);
        Local<Value> value;
        if (!JSON::Parse(context, toV8String(isolate, content)).ToLocal(&value)) {
            if (tryCatch.HasCaught()) {
                auto errStr = formatJsonError(context, tryCatch.Exception(), modulePath);
                auto v8Str = toV8String(isolate, errStr);
                resolver->Reject(context, Exception::SyntaxError(v8Str)).Check();
            } else {
                resolver->Reject(context, exception).Check();
            }
            return;
        }
        auto obj = Object::New(isolate);
        obj->DefineOwnProperty(
            context,
            toV8String(isolate, "default"),
            value,
            static_cast<PropertyAttribute>(v8::DontDelete)
        ).Check();
        obj->DefineOwnProperty(
            context,
            Symbol::GetToStringTag(isolate),
            toV8String(isolate, "Module"),
            static_cast<PropertyAttribute>(v8::ReadOnly | v8::DontEnum | v8::DontDelete)
        ).Check();
        resolver->Resolve(context, obj).Check();
        return;
    }
    ScriptOrigin scriptOrigin(
        isolate,
        toV8String(isolate, modulePath),
        0,
        0,
        false,
        -1,
        Local<Value>(),
        false,
        false,
        true,
        Local<Data>()
    );
    ScriptCompiler::Source source(toV8String(isolate, content), scriptOrigin);
    TryCatch tryCatch(isolate);
    Local<Module> module;
    if (!ScriptCompiler::CompileModule(isolate, &source).ToLocal(&module)) {
        if (tryCatch.HasCaught()) {
            resolver->Reject(context, tryCatch.Exception()).Check();
        } else {
            resolver->Reject(context, exception).Check();
        }
        return;
    }
    esModule->setModulePath(module, std::move(modulePath));
    if (module->InstantiateModule(context, resolveModuleCallback).FromMaybe(false)) {
        resolver->Resolve(context, module->GetModuleNamespace()).Check();
        Local<Value> value;
        if (!module->Evaluate(context).ToLocal(&value)) {
            auto errStr = formatException(context, exception);
            eprintln(errStr);
        }
        return;
    }
    if (tryCatch.HasCaught()) {
        resolver->Reject(context, tryCatch.Exception()).Check();
    } else {
        resolver->Reject(context, exception).Check();
    }
}

void importMetaObjectResolve(const FunctionCallbackInfo<Value>& info) {
    auto isolate = info.GetIsolate();
    HandleScope handleScope(isolate);
    auto context = isolate->GetCurrentContext();
    auto resolver = Promise::Resolver::New(context).ToLocalChecked();
    auto promise = resolver->GetPromise();
    if (info.Length() < 1) {
        auto v8Str = toV8String(isolate, "1 argument required, but only 0 present");
        resolver->Reject(context, Exception::TypeError(v8Str)).Check();
        info.GetReturnValue().Set(promise);
        return;
    }
    auto specifier = toBString(context, info[0]);
    if (!isLocalPath(specifier)) {
        auto errStr = BString::format(
            "'{}' not absolute path, or not prefixed with ./ or ../",
            specifier
        );
        auto v8Str = toV8String(isolate, errStr);
        resolver->Reject(context, Exception::TypeError(v8Str)).Check();
        info.GetReturnValue().Set(promise);
        return;
    }
    if (isAbsolutePath(specifier)) {
        BString filePath;
        filePath.reserve(7 + specifier.length());
        filePath += "file://";
        filePath += cleanPath(specifier);
        resolver->Resolve(context, toV8String(isolate, filePath)).Check();
    } else {
        auto modulePath = toBString(context, info.Data());
        auto moduleDir = dirname(modulePath);
        BString filePath;
        filePath.reserve(7 + moduleDir.length() + 1 + specifier.length());
        filePath += "file://";
        filePath += joinPath(moduleDir, specifier);
        resolver->Resolve(context, toV8String(isolate, filePath)).Check();
    }
    info.GetReturnValue().Set(promise);
}

}

namespace kun {

EsModule::EsModule(Environment* env) : env(env) {
    modulePathMap.reserve(256);
    depsPathMap.reserve(256);
}

bool EsModule::execute(const BString& path) {
    auto isolate = env->getIsolate();
    HandleScope handleScope(isolate);
    auto context = env->getContext();
    auto moduleDir = dirname(path);
    auto depsJsonPath = joinPath(moduleDir, "deps.json");
    if (!loadDeps(depsJsonPath)) {
        return false;
    }
    auto content = readFile(path).expect("Module not found '{}'", path);
    ScriptOrigin scriptOrigin(
        isolate,
        toV8String(isolate, path),
        0,
        0,
        false,
        -1,
        Local<Value>(),
        false,
        false,
        true,
        Local<Data>()
    );
    ScriptCompiler::Source source(toV8String(isolate, content), scriptOrigin);
    TryCatch tryCatch(isolate);
    Local<Module> module;
    if (!ScriptCompiler::CompileModule(isolate, &source).ToLocal(&module)) {
        if (tryCatch.HasCaught()) {
            auto errStr = formatException(context, tryCatch.Exception());
            eprintln(errStr);
        } else {
            eprintln("ERROR: Failed to compile module '{}'", path);
        }
        return false;
    }
    modulePathMap.emplace(module->GetIdentityHash(), path);
    if (module->InstantiateModule(context, resolveModuleCallback).FromMaybe(false)) {
        Local<Promise> promise;
        if (Local<Value> value; module->Evaluate(context).ToLocal(&value)) {
            promise = value.As<Promise>();
        } else {
            KUN_LOG_ERR("EsModule::execute");
        }
        env->runMicrotask();
        auto stalled = module->GetStalledTopLevelAwaitMessages(isolate);
        const auto& messages = stalled.second;
        for (const auto& message : messages) {
            auto errStr = formatMessage(context, message);
            if (!errStr.empty()) {
                eprintln(errStr);
            }
        }
        return !promise.IsEmpty() && promise->State() == Promise::kFulfilled;
    }
    if (tryCatch.HasCaught()) {
        auto errStr = formatException(context, tryCatch.Exception());
        eprintln(errStr);
    } else {
        eprintln("ERROR: Failed to evaluate module '{}'", path);
    }
    return false;
}

bool EsModule::loadDeps(const BString& path) {
    BString content;
    if (auto result = readFile(path)) {
        content = result.unwrap();
    } else {
        return true;
    }
    auto isolate = env->getIsolate();
    HandleScope handleScope(isolate);
    auto context = env->getContext();
    Local<Value> value;
    if (
        !JSON::Parse(context, toV8String(isolate, content)).ToLocal(&value) ||
        value->IsNull() ||
        !value->IsObject()
    ) {
        return false;
    }
    auto rootObj = value.As<Object>();
    Local<Object> depsObj;
    if (!fromObject(context, rootObj, "deps", depsObj)) {
        return true;
    }
    Local<Array> names;
    if (!depsObj->GetOwnPropertyNames(context).ToLocal(&names)) {
        return false;
    }
    auto depsDir = env->getDepsDir();
    auto len = names->Length();
    for (decltype(len) i = 0; i < len; i++) {
        BString name;
        if (!fromObject(context, names, i, name)) {
            continue;
        }
        Local<Object> obj;
        if (!fromObject(context, depsObj, name, obj)) {
            continue;
        }
        BString version;
        if (!fromObject(context, obj, "version", version)) {
            continue;
        }
        BString url;
        if (!fromObject(context, obj, "url", url)) {
            continue;
        }
        auto begin = url.find("//");
        if (begin == BString::END || begin + 2 == url.length()) {
            continue;
        }
        auto end = url.find("/", begin + 2);
        auto domain = url.substring(begin + 2, end);
        auto index = domain.find(":");
        if (index != BString::END) {
            BString str(domain.data(), domain.length());
            str[index] = '_';
            domain = std::move(str);
        }
        auto moduleDir = joinPath(depsDir, domain, name, version);
        depsPathMap.emplace(std::move(name), std::move(moduleDir));
    }
    return true;
}

void EsModule::setModulePath(Local<Module> module, const BString& path) {
    modulePathMap.insert_or_assign(module->GetIdentityHash(), path);
}

void EsModule::setModulePath(Local<Module> module, BString&& path) {
    modulePathMap.insert_or_assign(module->GetIdentityHash(), std::move(path));
}

Result<BString> EsModule::findModulePath(Local<Module> module) const {
    auto iter = modulePathMap.find(module->GetIdentityHash());
    if (iter != modulePathMap.end()) {
        return BString::view(iter->second);
    }
    return SysErr::err("Module not found");
}

Result<BString> EsModule::findDepsPath(const BString& specifier) const {
    auto index = specifier.find("/");
    if (index == BString::END) {
        return SysErr::err("Malformed specifier");
    }
    index = specifier.find("/", index + 1);
    if (index == BString::END || index + 1 == specifier.length()) {
        return SysErr::err("Malformed specifier");
    }
    auto name = specifier.substring(0, index);
    auto suffix = specifier.substring(index + 1);
    auto iter = depsPathMap.find(name);
    if (iter == depsPathMap.end()) {
        return SysErr::err("Dependency not found");
    }
    auto path = joinPath(iter->second, suffix);
    if (!pathExists(path)) {
        return SysErr::err("Dependency not exists");
    }
    return path;
}

namespace esm {

MaybeLocal<Promise> importModuleDynamicallyCallback(
    Local<Context> context,
    Local<Data> options,
    Local<Value> resourceName,
    Local<String> specifier,
    Local<FixedArray> importAttrs
) {
    auto isolate = context->GetIsolate();
    EscapableHandleScope handleScope(isolate);
    auto resolver = Promise::Resolver::New(context).ToLocalChecked();
    auto promise = resolver->GetPromise();
    auto data = new DynamicModuleData();
    data->isolate = isolate;
    data->context.Reset(isolate, context);
    data->options.Reset(isolate, options);
    data->resourceName.Reset(isolate, resourceName);
    data->specifier.Reset(isolate, specifier);
    data->importAttrs.Reset(isolate, importAttrs);
    data->resolver.Reset(isolate, resolver);
    auto specifierStr = toBString(context, specifier);
    auto errStr = BString::format("Failed to import '{}'", specifierStr);
    auto exception = Exception::Error(toV8String(isolate, errStr));
    data->exception.Reset(isolate, exception);
    isolate->EnqueueMicrotask(doImportModuleDynamically, data);
    return handleScope.Escape(promise);
}

void importMetaObjectCallback(Local<Context> context, Local<Module> module, Local<Object> meta) {
    auto isolate = context->GetIsolate();
    HandleScope handleScope(isolate);
    auto env = Environment::from(context);
    auto esModule = env->getEsModule();
    BString modulePath;
    if (auto result = esModule->findModulePath(module)) {
        modulePath = result.unwrap();
    } else {
        throwError(isolate, "Module not found");
        return;
    }
    BString filePath;
    filePath.reserve(7 + modulePath.length());
    filePath += "file://";
    filePath += modulePath;
    auto url = toV8String(isolate, filePath);
    auto resolve = Function::New(
        context,
        importMetaObjectResolve,
        toV8String(isolate, modulePath)
    ).ToLocalChecked();
    meta->Set(context, toV8String(isolate, "url"), url).Check();
    meta->Set(context, toV8String(isolate, "resolve"), resolve).Check();
}

}

}
