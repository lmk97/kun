#include "util/constants.h"

#ifdef KUN_PLATFORM_WIN32
#include <stdlib.h>
#include <wchar.h>
#include <windows.h>
#include <winsock2.h>
#include <vector>
#include "util/scope_guard.h"
#include "win/util.h"
#endif

#include "env/cmdline.h"
#include "env/environment.h"
#include "util/bstring.h"
#include "util/util.h"

using kun::BString;
using kun::Cmdline;
using kun::Environment;
using kun::ExposedScope;

#ifdef KUN_PLATFORM_UNIX
int main(int argc, char** argv) {
    Cmdline cmdline(argc, argv);
    Environment env(&cmdline);
    env.run(ExposedScope::MAIN);
    return 0;
}
#else
int wmain(int argc, wchar_t** argv) {
    WSADATA wsaData;
    int rc = ::WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (rc != 0) {
        auto errStr = BString::format("WSAStartup ({})", rc);
        KUN_LOG_ERR(errStr);
        return EXIT_FAILURE;
    }
    if (HIBYTE(wsaData.wVersion) != 2 || LOBYTE(wsaData.wVersion) != 2) {
        KUN_LOG_ERR("winsock 2.2 is not supported");
        return EXIT_FAILURE;
    }
    ON_SCOPE_EXIT {
        if (::WSACleanup() == SOCKET_ERROR) {
            auto errCode = ::WSAGetLastError();
            auto errStr = BString::format("WSACleanup ({})", errCode);
            KUN_LOG_ERR(errStr);
        }
    };
    std::vector<BString> args;
    args.reserve(argc);
    auto datas = new char*[argc];
    ON_SCOPE_EXIT {
        delete[] datas;
    };
    for (int i = 0; i < argc; i++) {
        args[i] = kun::win::toBString(argv[i], wcslen(argv[i])).unwrap();
        datas[i] = const_cast<char*>(args[i].c_str());
    }
    Cmdline cmdline(argc, datas);
    Environment env(&cmdline);
    env.run(ExposedScope::MAIN);
    return 0;
}
#endif
