#include <stdlib.h>
#include <string>
#include <tr1/functional>

#include "src/qrpc/util/log.h"
#include "src/qrpc/util/compiler.h"
#include "src/qrpc/util/thread.h"
#include "src/qrpc/rpc/errno.h"
#include "src/qrpc/rpc/builtin.h"
#include "src/qrpc/rpc/server.h"
#include "src/qrpc/rpc/server_impl.h"

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
#define NULL_RET(param)                 \
do {                                    \
    if (!param.empty())                 \
        break;                          \
    LOG(ERROR) << "invalid: " << #param;\
    return false;                       \
} while (0)

inline bool options_ok(const ServerOptions &opt)
{
    ZERO_RET(opt.rbuf_size);
    ZERO_RET(opt.sbuf_size);

    ZERO_RET(opt.min_rbuf_size);
    ZERO_RET(opt.max_rbuf_size);

    ZERO_RET(opt.min_sbuf_size);
    ZERO_RET(opt.max_sbuf_size);

    ZERO_RET(opt.keep_alive_time);
    ZERO_RET(opt.num_worker_thread);

    return true;
}

#undef ZERO_RET
#undef NULL_RET

void InitWorker(Thread *thr)
{
    DLOG(INFO) << "init worker ("
        << thr->name() << ", " << thr->id() << ")";
}

void ExitWorker(Thread *thr)
{
    DLOG(INFO) << "exit worker ("
        << thr->name() << ", " << thr->id() << ")";
}

} // anonymous namespace

ServerOptions::ServerOptions()
    : rbuf_size(16 * 1024)
    , sbuf_size(16 * 1024)
    , min_rbuf_size(32 * 1024)
    , max_rbuf_size(1024 * 1024)
    , min_sbuf_size(32 * 1024)
    , max_sbuf_size(1024 * 1024)
    , keep_alive_time(3600)
    , num_worker_thread(8)
    , init_cb(tr1::bind(InitWorker, tr1::placeholders::_1))
    , exit_cb(tr1::bind(ExitWorker, tr1::placeholders::_1))
{

}

Server::~Server()
{

}

int Server::New(const ServerOptions &options, event_base *base, Server **srvptr)
{
    if (!options_ok(options)) {
        return kErrParam;
    }

    if (!base) {
        LOG(WARNING) << "the event base is null";
    }

    Server *server = new ServerImpl(options, base);
    if (!server) {
        LOG(ERROR) << "alloc server object failed!!!";
        return kErrMem;
    }

    *srvptr = server;
    return kOk;
}

} // namespace qrpc
