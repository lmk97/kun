#include "runtime/http_server_channel.h"

#include "util/constants.h"

#if defined(KUN_PLATFORM_UNIX)
#include <sys/socket.h>
#elif defined(KUN_PLATFORM_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#endif

#include "loop/event_loop.h"
#include "util/scope_guard.h"
#include "sys/io.h"
#include "win/err.h"
#include "runtime/http_server_conn.h"

using kun::HttpServerConn;
using kun::sys::eprintln;
using kun::sys::setNonblocking;

namespace kun {

HttpServerChannel::HttpServerChannel(Environment* env) :
    Channel(KUN_INVALID_FD, ChannelType::READ),
    env(env)
{

}

void HttpServerChannel::onReadable() {
    /*KUN_FD_TYPE cfd = KUN_INVALID_FD;
    sockaddr_in caddr;
    socklen_t caddrLen = sizeof(caddr);
    while (true) {
        cfd = ::accept(fd, reinterpret_cast<sockaddr*>(&caddr), &caddrLen);
        if (cfd == KUN_INVALID_FD) {
            #if defined(KUN_PLATFORM_UNIX)
            if (errno == EINTR) {
                continue;
            }
            #elif defined(KUN_PLATFORM_WIN32)
            if (::WSAGetLastError() == WSAEINTR) {
                continue;
            }
            #endif
            break;
        }
        eprintln(
            "==== accept fd: {}, ip: {}, port: {}\n",
            cfd, inet_ntoa(caddr.sin_addr), ntohs(caddr.sin_port)
        );
        auto conn = new HttpServerConn(env, cfd);
        auto eventLoop = env->getEventLoop();
        if (!eventLoop->addChannel(conn)) {
            eprintln("====== add http server conn failed ====");
        }
    }*/
}

Result<bool> HttpServerChannel::listen(const BString& ip, int port, int backlog) {
    /*fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd == KUN_INVALID_FD) {
        auto errCode = win::convertError(::WSAGetLastError());
        return SysErr(errCode);
    }
    if (!setNonblocking(fd).unwrap()) {
        return false;
    }
    int optval = 1;
    ::setsockopt(
        fd,
        SOL_SOCKET,
        SO_REUSEADDR,
        reinterpret_cast<char*>(&optval),
        sizeof(optval)
    );
    sockaddr_in saddr;
    saddr.sin_family = AF_INET;
    saddr.sin_addr.s_addr = htonl(INADDR_ANY);
    saddr.sin_port = htons(static_cast<u_short>(port));
    int rc = bind(fd, reinterpret_cast<sockaddr*>(&saddr), sizeof(saddr));
    if (rc == SOCKET_ERROR) {
        auto errCode = win::convertError(::WSAGetLastError());
        return SysErr(errCode);
    }
    rc = ::listen(fd, backlog);
    if (rc == SOCKET_ERROR) {
        auto errCode = win::convertError(::WSAGetLastError());
        return SysErr(errCode);
    }
    auto eventLoop = env->getEventLoop();
    return eventLoop->addChannel(this);*/
    return true;
}

}
