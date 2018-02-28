#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <event.h>
#include <pthread.h>
#include <string>

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include "src/qrpc/util/log.h"
#include "src/qrpc/util/atomic.h"
#include "src/qrpc/util/random.h"
#include "src/qrpc/util/completion.h"
#include "src/qrpc/util/socket.h"
#include "src/qrpc/rpc/errno.h"
#include "src/qrpc/rpc/closure.h"
#include "src/qrpc/rpc/controller.h"
#include "src/qrpc/rpc/controller_client.h"
#include "src/qrpc/rpc/controller_server.h"
#include "src/qrpc/rpc/message.h"
#include "src/qrpc/rpc/connection.h"

using namespace std;
using namespace google::protobuf;

namespace qrpc {

ServerController::ServerController(ServerMessage *srv_msg)
    : tid_(pthread_self())
    , srv_msg_(srv_msg)
    , code_(0)
    , cancel_(false)
    , closure_(NULL)
{

}

ServerController::~ServerController()
{
    assert(closure_ == NULL);
}

string ServerController::LocalAddress() const
{
    if (tid_ != pthread_self()) {
        LOG(FATAL) << "the RPC is running in other thread context";
    }

    ServerConnection *conn = srv_msg_->server_connection();
    return conn->local_addr();
}

string ServerController::RemoteAddress() const
{
    if (tid_ != pthread_self()) {
        LOG(FATAL) << "the RPC is running in other thread context";
    }

    ServerConnection *conn = srv_msg_->server_connection();
    return conn->remote_addr();
}

void ServerController::Reset()
{
    LOG(FATAL) << "client-side method";
}

bool ServerController::Failed() const
{
    LOG(FATAL) << "client-side method";
    return false;
}

string ServerController::ErrorText() const
{
    LOG(FATAL) << "client-side method";
    return "";
}

void ServerController::StartCancel()
{
    LOG(FATAL) << "client-side method";
}

void ServerController::SetFailed(const string &reason)
{
    if (tid_ != pthread_self()) {
        LOG(FATAL) << "the RPC is running in other thread context";
    }

    code_ = kErrUserDef;
    error_text_ = reason;
}

bool ServerController::IsCanceled() const
{
    if (tid_ != pthread_self()) {
        LOG(FATAL) << "the RPC is running in other thread context";
    }

    return cancel_;
}

void ServerController::NotifyOnCancel(google::protobuf::Closure *callback)
{
    if (tid_ != pthread_self()) {
        LOG(FATAL) << "the RPC is running in other thread context";
    }

    if (closure_) {
        LOG(FATAL) << "Notifyoncancel has been called";
    }

    if (cancel_) {
        callback->Run();
    } else {
        closure_ = callback;
    }
}

} // namespace qrpc
