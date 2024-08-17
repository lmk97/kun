#ifndef KUN_RUNTIME_HTTP_SERVER_CONN_H
#define KUN_RUNTIME_HTTP_SERVER_CONN_H

#include "env/environment.h"
#include "loop/channel.h"
#include "util/bstring.h"
#include "util/type_def.h"

namespace kun {

class HttpServerConn : public Channel {
public:
    HttpServerConn(Environment* env, KUN_FD_TYPE fd);

    void onReadable() override final;

private:
    Environment* env;
};

}

#endif
