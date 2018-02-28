#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <vector>
#include <string>
#include <tr1/functional>

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include "src/qrpc/util/log.h"
#include "src/qrpc/util/atomic.h"
#include "src/qrpc/util/socket.h"
#include "src/qrpc/util/compiler.h"
#include "src/qrpc/util/completion.h"
#include "src/qrpc/util/thread.h"
#include "src/qrpc/util/event_queue.h"
#include "src/qrpc/rpc/errno.h"
#include "src/qrpc/rpc/closure.h"
#include "src/qrpc/rpc/command.h"
#include "src/qrpc/rpc/controller.h"
#include "src/qrpc/rpc/controller_client.h"
#include "src/qrpc/rpc/controller_server.h"
#include "src/qrpc/rpc/message.h"
#include "src/qrpc/rpc/connection.h"
#include "src/qrpc/rpc/compressor.h"
#include "src/qrpc/rpc/builtin.h"
#include "src/qrpc/rpc/server.h"
#include "src/qrpc/rpc/server_impl.h"
#include "src/qrpc/rpc/worker.h"

using namespace std;

namespace qrpc {

namespace {

string new_thread_name()
{
    static atomic_t stid = ATOMIC_INIT(0);
    int tid = atomic_inc_return(&stid);

    char tmp[17] = { 0 };
    snprintf(tmp, 17, "[rpc/slave%02d]", tid);

    return string(tmp);
}

} // anonymous namespace

Worker::Worker(ServerImpl *server)
    : server_(server)
    , compressor_(NULL)
    , bg_thread_(NULL)
{
    bg_thread_ = new Thread(new_thread_name(),
            tr1::bind(&Worker::InitWorker, this, tr1::placeholders::_1),
            tr1::bind(&Worker::ExitWorker, this, tr1::placeholders::_1));
    if (!bg_thread_) {
        LOG(FATAL) << "create worker thread failed!!!";
    }
}

Worker::~Worker()
{
    delete bg_thread_;

    assert(clients_.empty() == true);
}

void Worker::InitWorker(Thread *thr)
{
    /* create thread based compressor */
    compressor_ = new Compressor();
    if (!compressor_) {
        LOG(FATAL) << "create compressor failed!!!";
    }

    const ServerOptions &opt = server_->options();
    opt.init_cb(thr);
}

void Worker::ExitWorker(Thread *thr)
{
    /* user should free resources */
    const ServerOptions &opt = server_->options();
    opt.exit_cb(thr);

    while (!clients_.empty()) {
        ClientQueue::iterator it = clients_.begin();
        it->second->Close();
    }
    delete compressor_;
}

void Worker::Link(::qrpc::Link *cmd)
{
    EvQueue *evq = bg_thread_->ev_queue();
    evq->Push(cmd);
}

void Worker::HandleLink(::qrpc::Link *cmd)
{
    int sfd = cmd->sfd_;
    string local = cmd->local_;
    string remote = cmd->remote_;
    delete cmd;

    ServerConnection *conn = new ServerConnection(this,
            sfd, local, remote);
    if (!conn) {
        close(sfd);
        LOG(ERROR) << "alloc server connection failed!!!";
        return;
    }

    clients_.insert(Client(conn, conn));
}

void Worker::Listen(::qrpc::Listen *cmd)
{
    EvQueue *evq = bg_thread_->ev_queue();
    evq->Push(cmd);
}

void Worker::HandleListen(::qrpc::Listen *cmd)
{
    ServerImpl *impl = cmd->impl_;

    if (!cmd->listen_) {
        /* remove listen event */
        impl->StopServer();
    } else {
        /* add listen event */
        cmd->res_ = impl->StartServer();
    }
    cmd->work_.Signal();
}

void Worker::Unlink(ServerConnection *conn)
{
    ClientQueue::iterator it;

    it = clients_.find(conn);
    if (it != clients_.end()) {
        clients_.erase(it);
    } else {
        LOG(FATAL) << "invalid client";
    }

    delete conn;
}

} // namespace qrpc
