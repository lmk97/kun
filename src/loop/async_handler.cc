#include "loop/async_handler.h"

#include <errno.h>
#include <stdint.h>

#include "v8.h"
#include "loop/event_loop.h"
#include "util/constants.h"
#include "util/utils.h"

#ifdef KUN_PLATFORM_UNIX
#include <unistd.h>
#endif

#if defined(KUN_PLATFORM_LINUX)
#include <sys/eventfd.h>
#elif defined(KUN_PLATFORM_WIN32)
#include <winsock2.h>
#include "win/err.h"
#endif

KUN_V8_USINGS;

namespace kun {

AsyncHandler::AsyncHandler(Environment* env) :
    Channel(KUN_INVALID_FD, ChannelType::READ),
    env(env),
    threadPool(this)
{
    #if defined(KUN_PLATFORM_LINUX)
    fd = ::eventfd(0, EFD_CLOEXEC | EFD_NONBLOCK);
    if (fd == -1) {
        KUN_LOG_ERR(errno);
    }
    #elif defined(KUN_PLATFORM_WIN32)
    fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd == INVALID_SOCKET) {
        auto errCode = win::convertError(::WSAGetLastError());
        KUN_LOG_ERR(errCode);
    }
    #endif
}

AsyncHandler::~AsyncHandler() {
    if (!tryClose()) {
        KUN_LOG_ERR("Failed to close AsyncHandler");
    }
}

void AsyncHandler::onReadable() {
    #if defined(KUN_PLATFORM_LINUX)
    uint64_t value;
    if (::read(fd, &value, sizeof(value)) == -1) {
        KUN_LOG_ERR(errno);
    }
    #elif defined(KUN_PLATFORM_WIN32)
    auto eventLoop = env->getEventLoop();
    if (!eventLoop->removeChannel(this)) {
        KUN_LOG_ERR("Failed to remove AsyncHandler");
    }
    fd = ::socket(AF_INET, SOCK_DGRAM, 0);
    if (fd != INVALID_SOCKET) {
        if (!eventLoop->addChannel(this)) {
            KUN_LOG_ERR("Failed to add AsyncHandler");
        }
    } else {
        auto errCode = win::convertError(::WSAGetLastError());
        KUN_LOG_ERR(errCode);
    }
    #endif
    auto isolate = env->getIsolate();
    HandleScope handleScope(isolate);
    auto context = env->getContext();
    auto requests = threadPool.getResolvedRequests();
    for (auto& req : requests) {
        req.resolve(context);
    }
}

void AsyncHandler::notify() {
    #if defined(KUN_PLATFORM_LINUX)
    uint64_t value = 1;
    if (::write(fd, &value, sizeof(value)) == -1) {
        KUN_LOG_ERR(errno);
    }
    #elif defined(KUN_PLATFORM_WIN32)
    if (::closesocket(fd) == SOCKET_ERROR) {
        auto errCode = win::convertError(::WSAGetLastError());
        KUN_LOG_ERR(errCode);
    }
    #endif
}

}
