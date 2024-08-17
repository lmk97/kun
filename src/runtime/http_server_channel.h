#ifndef KUN_RUNTIME_HTTP_SERVER_CHANNEL_H
#define KUN_RUNTIME_HTTP_SERVER_CHANNEL_H

#include "env/environment.h"
#include "loop/channel.h"
#include "util/bstring.h"
#include "util/result.h"

namespace kun {

class HttpServerChannel : public Channel {
public:
    HttpServerChannel(Environment* env);

    ~HttpServerChannel() = default;

    void onReadable() override final;

    Result<bool> listen(const BString& ip, int port, int backlog);

private:
    Environment* env;
};

}

#endif
