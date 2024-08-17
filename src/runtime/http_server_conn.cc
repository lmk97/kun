#include "runtime/http_server_conn.h"

#include <string.h>

#include "util/constants.h"

#if defined(KUN_PLATFORM_UNIX)
#include <sys/socket.h>
#elif defined(KUN_PLATFORM_WIN32)
#include <winsock2.h>
//#include <ws2tcpip.h>
#endif

#include "util/bstring.h"
#include "sys/io.h"
#include "loop/event_loop.h"

using kun::sys::eprintln;
using kun::sys::setNonblocking;

namespace kun {

HttpServerConn::HttpServerConn(Environment* env, KUN_FD_TYPE fd) :
    Channel(fd, ChannelType::READ),
    env(env)
{
    if (!setNonblocking(fd).unwrap()) {
        eprintln("ERROR: 'HttpServerConn' setNonblocking failed");
    }
}

void HttpServerConn::onReadable() {
    /*int rc = 0;
    char buf[4096];
    while (true) {
        rc = ::recv(fd, buf, sizeof(buf), 0);
        if (rc == SOCKET_ERROR) {
            auto errCode = ::WSAGetLastError();
            if (errCode == WSAEINTR) {
                continue;
            }
            if (errCode == WSAEWOULDBLOCK) {
                break;
            }
        }
        if (rc == 0 || rc == SOCKET_ERROR) {
            eprintln("==== fd: {} closed when reading\n", fd);
            auto eventLoop = env->getEventLoop();
            eventLoop->removeChannel(this);
            delete this;
            return;
        }
        buf[rc] = '\0';
        eprintln("==== recv data:\n{}\n", buf);
    }
    auto content = "hello world!";
    auto data = BString::format(
        "HTTP/1.1 200 OK\r\nContent-Length: {}\r\n\r\n{}",
        strlen(content),
        content
    );
    rc = send(fd, data.data(), data.length(), 0);
    if (rc == 0) {
        eprintln("==== fd: {} closed when writing\n", fd);
        auto eventLoop = env->getEventLoop();
        eventLoop->removeChannel(this);
        delete this;
    }*/
}

}
