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

using namespace std;
using namespace google::protobuf;

namespace qrpc {

ClientController::ClientController(const ControllerOptions &options)
    : tid_(0)
    , options_(options)
    , code_(0)
    , client_message_(NULL)
{

}

ClientController::~ClientController()
{

}

string ClientController::LocalAddress() const
{
    if (tid_ != pthread_self()) {
        LOG(FATAL) << "the RPC is running in other thread context";
    }

    return local_addr_;
}

string ClientController::RemoteAddress() const
{
    if (tid_ != pthread_self()) {
        LOG(FATAL) << "the RPC is running in other thread context";
    }

    return remote_addr_;
}

void ClientController::Reset()
{
    if (client_message_) {
        LOG(FATAL) << "the RPC is in progress";
    }

    tid_ = 0;

    code_ = 0;
    error_text_ = "";

    local_addr_ = "";
    remote_addr_ = "";

    client_message_ = NULL;
}

bool ClientController::Failed() const
{
    if (client_message_) {
        LOG(FATAL) << "the RPC is in progress";
    }

    return (code_ != 0);
}

string ClientController::ErrorText() const
{
    if (client_message_) {
        LOG(FATAL) << "the RPC is in progress";
    }

    switch (code_) {
    case kErrUserDef:
        return error_text_;
    case kOk:
        return "";
    default:
        return rerror(code_);
    }
}

void ClientController::StartCancel()
{
    if (!client_message_) {
        LOG(FATAL) << "the controller is initial state";
    }
    if (tid_ != pthread_self()) {
        LOG(FATAL) << "the RPC is running in other thread context";
    }

    client_message_->StartCancel();
}

void ClientController::SetFailed(const string &reason)
{
    LOG(FATAL) << "server-side method";
}

bool ClientController::IsCanceled() const
{
    LOG(FATAL) << "server-side method";
    return false;
}

void ClientController::NotifyOnCancel(google::protobuf::Closure *callback)
{
    LOG(FATAL) << "server-side method";
}

} // namespace qrpc
