#include <stdlib.h>
#include <string>

#include "src/qrpc/util/log.h"
#include "src/qrpc/util/compiler.h"
#include "src/qrpc/rpc/errno.h"
#include "src/qrpc/rpc/closure.h"
#include "src/qrpc/rpc/controller.h"
#include "src/qrpc/rpc/controller_client.h"
#include "src/qrpc/rpc/channel.h"
#include "src/qrpc/rpc/channel_impl.h"

using namespace std;

namespace qrpc {

namespace {

#define ZERO_RET(param)                 \
do {                                    \
    if ((param) > 0)                    \
        break;                          \
    LOG(ERROR) << "invalid: " << #param;\
    return false;                       \
} while (0)
#define NEGATIVE_RET(param)             \
do {                                    \
    if ((param) >= 0)                   \
        break;                          \
    LOG(ERROR) << "invalid: " << #param;\
    return false;                       \
} while (0)
#define NULL_RET(param)                 \
do {                                    \
    if (!param.empty())                 \
        break;                          \
    LOG(ERROR) << "invalid: " << #param;\
    return false;                       \
} while (0)

inline bool options_ok(const ChannelOptions &opt)
{
    ZERO_RET(opt.rbuf_size);
    ZERO_RET(opt.sbuf_size);

    ZERO_RET(opt.min_rbuf_size);
    ZERO_RET(opt.max_rbuf_size);

    ZERO_RET(opt.min_sbuf_size);
    ZERO_RET(opt.max_sbuf_size);

    ZERO_RET(opt.connect_timeout);
    ZERO_RET(opt.retry_interval);
    NEGATIVE_RET(opt.heartbeat_interval);

    return true;
}

#undef ZERO_RET
#undef NULL_RET
#undef NEGATIVE_RET

} // anonymous namespace

ChannelOptions::ChannelOptions()
    : rbuf_size(16 * 1024)
    , sbuf_size(16 * 1024)
    , min_rbuf_size(32 * 1024)
    , max_rbuf_size(1024 * 1024)
    , min_sbuf_size(32 * 1024)
    , max_sbuf_size(1024 * 1024)
    , connect_timeout(5000)
    , retry_interval(1000)
    , heartbeat_interval(600000)
{

}

Channel::~Channel()
{

}

int Channel::New(const ChannelOptions &options, 
                 const string &host, int port,
                 event_base *base, Channel **chanptr)
{
    if (!options_ok(options)) {
        return kErrParam;
    }

    if (host.empty()) {
        LOG(ERROR) << "host address is empty";
        return kErrParam;
    }

    if (port <= 0) {
        LOG(ERROR) << "network port is invalid";
        return kErrParam;
    }

    if (!base) {
        LOG(ERROR) << "event base is null";
        return kErrParam;
    }

    Channel *channel = new ChannelImpl(options, host, port, base);
    if (!channel) {
        LOG(ERROR) << "alloc channel object failed!!!";
        return kErrMem;
    }

    *chanptr = channel;
    return kOk;
}

} // namespace qrpc
