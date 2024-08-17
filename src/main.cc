#ifdef KUN_PLATFORM_WIN32
#include <winsock2.h>
#endif

#include <thread>

#include "v8.h"
#include "libplatform/libplatform.h"
#include "util/constants.h"
#include "util/scope_guard.h"
#include "util/min_heap.h"
#include "sys/io.h"
#include "sys/time.h"
#include "sys/path.h"
#include "sys/process.h"
#include "env/environment.h"
#include "loop/event_loop.h"
#include "runtime/http_server_channel.h"
#include "loop/channel.h"
#include "loop/timer.h"

using v8::Isolate;
using v8::Context;
using v8::HandleScope;
using v8::ArrayBuffer;
using v8::ObjectTemplate;
using kun::Options;
using kun::Environment;
using kun::EventLoop;
using kun::ChannelType;
using kun::Channel;
using kun::Timer;
using kun::HttpServerChannel;
using kun::MinHeapData;
using kun::MinHeap;
using kun::sys::eprintln;

class TestTimer : public Timer {
public:
    TestTimer(uint64_t milliseconds, bool interval) :
        Timer(milliseconds, interval) {}

    void onReadable() override final;
};

void TestTimer::onReadable() {
    eprintln("==== timer onreadable ====");
}

int main(int argc, char** argv) {
    #ifdef KUN_PLATFORM_WIN32
    WSADATA wsaData;
    int rc = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (rc != 0) {
        eprintln("ERROR: WSAStartup failed ({})", rc);
        return EXIT_FAILURE;
    }
    if (HIBYTE(wsaData.wVersion) != 2 || LOBYTE(wsaData.wVersion) != 2) {
        eprintln("ERROR: winsock2.2 is not supported");
        return EXIT_FAILURE;
    }
    ON_SCOPE_EXIT {
        int rc = ::WSACleanup();
        if (rc != 0) {
            eprintln("ERROR: WSACleanup failed ({})", rc);
        }
    };
    #endif

    auto platform = v8::platform::NewDefaultPlatform();
    v8::V8::InitializePlatform(platform.get());
    v8::V8::Initialize();
    Isolate::CreateParams createParams;
    createParams.array_buffer_allocator = ArrayBuffer::Allocator::NewDefaultAllocator();
    auto isolate = Isolate::New(createParams);
    {
        Isolate::Scope isolateScope(isolate);
        HandleScope handleScope(isolate);
        {
            auto objTmpl = ObjectTemplate::New(isolate);
            auto context = Context::New(isolate, nullptr, objTmpl);
            Context::Scope contextScope(context);
            eprintln("=====v8 version: {}", v8::V8::GetVersion());
            Options options(argc, argv);
            Environment env(&options);
            EventLoop eventLoop(&env);
            env.setEventLoop(&eventLoop);
            //HttpServerChannel httpServerChannel(&env);
            //httpServerChannel.listen("0.0.0.0", 8999, 511).unwrap();
            //TestTimer timer(3000, false);
            //eventLoop.addChannel(&timer);
            auto path = kun::sys::normalizePath("./e/f/g/./../../../../././///\\\\a/.\\../.\\../b/c\\.\\..\\.\\d/.\\//./\\/.\\////..\\\\//");
            eprintln("====path: {}", path);
            eventLoop.run();
        }
    }
    isolate->ContextDisposedNotification();
    isolate->LowMemoryNotification();
    isolate->ClearKeptObjects();
    isolate->Dispose();
    v8::V8::Dispose();
    v8::V8::DisposePlatform();
    delete createParams.array_buffer_allocator;
    return 0;
}
