#include "loop/async_event.h"

#include <stdint.h>
#include <errno.h>

#if defined(KUN_PLATFORM_LINUX)
#include <unistd.h>
#include <sys/eventfd.h>
#elif defined(KUN_PLATFORM_WIN32)
#include <winsock2.h>
#include "win/err.h"
#endif

#include "v8.h"
#include "env/options.h"
#include "sys/io.h"
#include "loop/event_loop.h"
#include "util/sys_err.h"

KUN_V8_USINGS;

using kun::sys::eprintln;

namespace kun {

AsyncEvent::AsyncEvent(Environment* env) :
    Channel(KUN_INVALID_FD, ChannelType::READ),
    env(env),
    threadPool(this)
{
    auto options = env->getOptions();
    auto size = options->get<int>(OptionName::THREAD_POOL_SIZE, 4);
    if (size <= 1) {
        size = 4;
    }
    threadPool.init(static_cast<uint32_t>(size));
#if defined(KUN_PLATFORM_LINUX)
    fd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (fd == -1) {
        auto [code, phrase] = SysErr(errno);
        eprintln("ERROR: 'eventfd' ({}) {}", code, phrase);
    }
#elif defined(KUN_PLATFORM_WIN32)
    fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == KUN_INVALID_FD) {
        auto errCode = win::convertError(::WSAGetLastError());
        auto [code, phrase] = SysErr(errCode);
        eprintln("ERROR: 'socket' ({}) {}", code, phrase);
    }
#endif
}

void AsyncEvent::onReadable() {
#if defined(KUN_PLATFORM_LINUX)
    uint64_t value;
    if (::read(fd, &value, sizeof(value)) == -1) {
        auto [code, phrase] = SysErr(errno);
        eprintln("ERROR: 'read' ({}) {}", code, phrase);
    }
#elif defined(KUN_PLATFORM_WIN32)
    auto eventLoop = env->getEventLoop();
    if (!eventLoop->removeChannel(this)) {
        eprintln("ERROR: 'AsyncEvent.onReadable' failed to remove channel");
    }
    fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd != KUN_INVALID_FD) {
        if (!eventLoop->addChannel(this)) {
            eprintln("ERROR: 'AsyncEvent.notify' failed to add channel");
        }
    } else {
        auto errCode = win::convertError(::WSAGetLastError());
        auto [code, phrase] = SysErr(errCode);
        eprintln("ERROR: 'socket' ({}) {}", code, phrase);
    }
#endif
    auto isolate = env->getIsolate();
    HandleScope handleScope(isolate);
    auto context = env->getContext();
    size_t count = 0;
    {
        std::lock_guard<std::mutex> lockGuard(notifyMutex);
        count = handledCount;
    }
    auto begin = asyncRequests.begin();
    auto end = begin + count;
    for (auto iter = begin; iter != end; ++iter) {
        iter->resolve(context);
    }
    std::lock_guard<std::mutex> lockGuard(notifyMutex);
    asyncRequests.erase(begin, end);
    handledCount -= count;
}

void AsyncEvent::notify() {
#if defined(KUN_PLATFORM_LINUX)
    uint64_t value;
    if (::read(fd, &value, sizeof(value)) == -1) {
        auto [code, phrase] = SysErr(errno);
        eprintln("ERROR: 'read' ({}) {}", code, phrase);
    }
#elif defined(KUN_PLATFORM_WIN32)
    if (::closesocket(fd) == SOCKET_ERROR) {
        auto errCode = win::convertError(::WSAGetLastError());
        auto [code, phrase] = SysErr(errCode);
        eprintln("ERROR: 'closesocket' ({}) {}", code, phrase);
    }
#endif
}

void AsyncEvent::submit(AsyncRequest&& asyncRequest) {
    std::lock_guard<std::mutex> lockGuard(handleMutex);
    asyncRequests.emplace_back(std::move(asyncRequest));
}

bool AsyncEvent::tryClose() {
    std::lock_guard<std::mutex> lockGuard(handleMutex);
    if (asyncRequests.empty()) {
        closed = true;
    }
    return closed;
}

}
