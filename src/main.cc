#include "util/constants.h"

#include <stdlib.h>

#if defined(KUN_PLATFORM_UNIX)
#include <errno.h>
#include <signal.h>
#elif defined(KUN_PLATFORM_WIN32)
#include <wchar.h>
#include <windows.h>
#include <winsock2.h>
#include "util/scope_guard.h"
#include "win/utils.h"
#endif

#include "env/cmdline.h"
#include "env/environment.h"
#include "util/bstring.h"
#include "util/utils.h"

using kun::BString;
using kun::Cmdline;
using kun::Environment;
using kun::ExposedScope;

#if defined(KUN_PLATFORM_UNIX)
int main(int argc, char** argv) {
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sa.sa_flags = 0;
    if (::sigemptyset(&sa.sa_mask) == -1) {
        KUN_LOG_ERR(errno);
        return EXIT_FAILURE;
    }
    if (::sigaction(SIGPIPE, &sa, nullptr) == -1) {
        KUN_LOG_ERR(errno);
        return EXIT_FAILURE;
    }
    Cmdline cmdline(argc, argv);
    Environment env(&cmdline);
    env.run(ExposedScope::MAIN);
    return 0;
}
#else
int wmain(int argc, wchar_t** wargv) {
    WSADATA wsaData;
    int rc = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (rc != 0) {
        KUN_LOG_ERR("WSAStartup failed with error {}", rc);
        return EXIT_FAILURE;
    }
    if (HIBYTE(wsaData.wVersion) != 2 || LOBYTE(wsaData.wVersion) != 2) {
        KUN_LOG_ERR("The winsock 2.2 is not supported");
        return EXIT_FAILURE;
    }
    ON_SCOPE_EXIT {
        if (::WSACleanup() == SOCKET_ERROR) {
            auto errCode = ::WSAGetLastError();
            KUN_LOG_ERR("WSACleanup failed with error {}", errCode);
        }
    };
    auto args = new BString[argc];
    auto argv = new char*[argc];
    ON_SCOPE_EXIT {
        delete[] argv;
        delete[] args;
    };
    for (int i = 0; i < argc; i++) {
        args[i] = kun::win::toBString(wargv[i], wcslen(wargv[i])).unwrap();
        argv[i] = const_cast<char*>(args[i].c_str());
    }
    Cmdline cmdline(argc, argv);
    Environment env(&cmdline);
    env.run(ExposedScope::MAIN);
    return 0;
}
#endif
