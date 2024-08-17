#ifndef KUN_LOOP_CHANNEL_H
#define KUN_LOOP_CHANNEL_H

#include "util/constants.h"
#include "util/type_def.h"
#include "sys/io.h"

namespace kun {

enum class ChannelType {
    READ = 1,
    WRITE = 2,
    TIMER = 4
};

class Channel {
public:
    Channel(const Channel&) = delete;

    Channel& operator=(const Channel&) = delete;

    Channel(Channel&&) = delete;

    Channel& operator=(Channel&&) = delete;

    Channel(KUN_FD_TYPE fd, ChannelType type) : fd(fd), type(type) {}

    virtual ~Channel() = 0;

    virtual void onReadable() {}

    virtual void onWritable() {}

    virtual void onError() {}

    KUN_FD_TYPE fd;
    ChannelType type;
};

inline Channel::~Channel() {
    if (fd != KUN_INVALID_FD) {
    #if defined(KUN_PLATFORM_UNIX)
        bool isSuccess = ::close(fd) == 0;
    #elif defined(KUN_PLATFORM_WIN32)
        bool isSuccess = ::closesocket(fd) == 0;
    #endif
        if (isSuccess) {
            fd = KUN_INVALID_FD;
        } else {
            sys::eprintln("ERROR: '~Channel' close failed");
        }
    }
}

}

#endif
