#include <sys/types.h>
#include <stdio.h>
#include <string>

#include "src/qrpc/util/log.h"
#include "src/qrpc/util/compiler.h"
#include "src/qrpc/rpc/errno.h"
#include "src/qrpc/rpc/closure.h"
#include "src/qrpc/rpc/controller.h"
#include "src/qrpc/rpc/controller_client.h"
#include "src/qrpc/rpc/controller_server.h"
#include "src/qrpc/rpc/channel.h"
#include "src/qrpc/rpc/channel_impl.h"
#include "src/qrpc/rpc/worker.h"
#include "src/qrpc/rpc/builtin.h"
#include "src/qrpc/rpc/server.h"
#include "src/qrpc/rpc/server_impl.h"
#include "src/qrpc/rpc/message.h"
#include "src/qrpc/rpc/message.pb.h"
#include "src/qrpc/rpc/connection.h"

using namespace std;
using namespace google::protobuf;

namespace qrpc {

// -------------------------------------------------------------
// class Message
// -------------------------------------------------------------

Message::~Message()
{

}

// -------------------------------------------------------------
// class ServerMessage
// -------------------------------------------------------------

ServerMessage::ServerMessage(ServerConnection *conn)
    : conn_(conn)
    , compression_type_(0)
    , request_(NULL)
    , response_(NULL)
    , service_(NULL)
    , method_(NULL)
    , controller_(this)
    , closure_(this, &ServerMessage::OnRpcDone, false)
{

}

ServerMessage::~ServerMessage()
{
    delete request_;
    delete response_;
}

void ServerMessage::OnRpcDone()
{
    pthread_t tid = controller_.thread_context();
    if (tid != pthread_self()) {
        LOG(FATAL) << "the RPC should run in the same thread context";
    }

    if (controller_.code()) {
        meta_.set_code(controller_.code());
        meta_.set_error_text(controller_.error_text());
    }
    
    conn_->Send(this);
}

int ServerMessage::CompressionType() const
{
    return compression_type_;
}

void ServerMessage::ByteSize(int *smeta, int *sdata) const
{
    assert(smeta != NULL);
    assert(sdata != NULL);

    *smeta = meta_.ByteSize();

    if (meta_.code()) {
        *sdata = 0;
    } else{
        *sdata = response_->ByteSize();
    }

    if ((uint32_t)*smeta > kMaxMetaSize) {
        LOG(FATAL) << "the message meta is too long";
    }
    if ((uint32_t)*sdata > kMaxDataSize) {
        LOG(FATAL) << "the message data is too long";
    }
}

bool ServerMessage::SerializeToArray(char *data, int len) const
{
    int smeta = meta_.GetCachedSize();
    if (smeta > len) {
        LOG(ERROR) << "the array is too small!!!";
        return false;
    }
    uint8 *brk = meta_.SerializeWithCachedSizesToArray((uint8 *)data);

    if (meta_.code()) {
        return true;
    }

    int sdata = response_->GetCachedSize();
    if (smeta + sdata > len) {
        LOG(ERROR) << "the array is too small!!!";
        return false;
    }
    response_->SerializeWithCachedSizesToArray(brk);

    return true;
}

bool ServerMessage::ParseFromArray(const char *data, int len, const MsgMeta &meta)
{
    Worker *worker = conn_->worker();
    ServerImpl *srv_impl = worker->server_impl();

    service_ = srv_impl->Find(meta);
    if (!service_) {
        LOG(ERROR) << "not register RPC server"
            << ", service: "
            << meta.service()
            << ", method: "
            << meta.method();
        return false;
    }

    method_ = service_->GetDescriptor()->FindMethodByName(meta.method());
    if (!method_) {
        LOG(ERROR) << "not implemente RPC method"
            << ", service: "
            << meta.service()
            << ", method: "
            << meta.method();
        return false;
    }

    request_  = service_->GetRequestPrototype(method_).New();
    response_ = service_->GetResponsePrototype(method_).New();
    if (!request_ || !response_) {
        LOG(FATAL) << "alloc message failed!!!";
    }

    compression_type_ = meta.compression_type();
    meta_.set_sequence(meta.sequence());

    return request_->ParseFromArray(data, len);
}

// -------------------------------------------------------------
// class ClientMessage
// -------------------------------------------------------------

ClientMessage::ClientMessage(ChannelImpl *channel,
                             ClientController *controller,
                             google::protobuf::Closure *done,
                             const google::protobuf::Message *request,
                             google::protobuf::Message *response,
                             const google::protobuf::MethodDescriptor *method)
    : finish_(false)
    , monitor_(false)
    , channel_(channel)
    , controller_(controller)
    , done_(done)
    , response_(response)
    , request_(request)
{
    const string &fname = method->full_name();
    size_t dotpos = fname.find_last_of('.');
    if (dotpos == string::npos) {
        LOG(FATAL) << "invalid method: " << fname;
    }
    string service = fname.substr(0, dotpos);

    meta_.set_sequence(channel->next_sequence());
    meta_.set_service(service);
    meta_.set_method(method->name());
    meta_.set_compression_type(controller->options().compression);

    controller->SetOwnership(this);
}

ClientMessage::~ClientMessage()
{
    assert(finish_ == true);
    assert(monitor_ == false);
}

void ClientMessage::AssignEndpoints()
{
    ClientConnection *conn = channel_->client_connection();

    if (conn == NULL) {
        char tmp[10] = { 0 };
        snprintf(tmp, 10, ":%d", channel_->port());
        string addr = channel_->host() + tmp;

        controller_->SetRemoteAddress(addr);
        controller_->SetLocalAddress(channel_->endpoint());
    } else {
        controller_->SetLocalAddress(conn->local_addr());
        controller_->SetRemoteAddress(conn->remote_addr());
    }
}

void ClientMessage::NewMonitor()
{
    if (monitor_) {
        timer_.SchedCancel();
    }

    monitor_ = true;

    const ControllerOptions &ctl_opt = controller_->options();

    timer_.Set(channel_->base(),
               ctl_opt.rpc_timeout,
               tr1::bind(&ClientMessage::HandleTimeout, this));
    timer_.SchedOneshot();
}

void ClientMessage::DelMonitor()
{
    if (!monitor_) {
        return;
    }

    monitor_ = false;
    timer_.SchedCancel();
}

void ClientMessage::HandleTimeout()
{
    controller_->SetResponseCode(kErrTimeout);

    monitor_ = false;
    channel_->OnRpcTimeout(this);
}

void ClientMessage::StartCancel()
{
    controller_->SetResponseCode(kErrCancel);

    channel_->StartCancel(this);
}

int ClientMessage::CompressionType() const
{
    return meta_.compression_type();
}

void ClientMessage::ByteSize(int *smeta, int *sdata) const
{
    assert(smeta != NULL);
    assert(sdata != NULL);

    *smeta = meta_.ByteSize();
    *sdata = request_->ByteSize();

    if ((uint32_t)*smeta > kMaxMetaSize) {
        LOG(FATAL) << "the message meta is too long";
    }
    if ((uint32_t)*sdata > kMaxDataSize) {
        LOG(FATAL) << "the message data is too long";
    }
}

bool ClientMessage::SerializeToArray(char *data, int len) const
{
    int smeta = meta_.GetCachedSize();
    int sdata = request_->GetCachedSize();

    if (smeta + sdata > len) {
        LOG(ERROR) << "the array is too small!!!";
        return false;
    }

    uint8 *brk = meta_.SerializeWithCachedSizesToArray((uint8 *)data);
    request_->SerializeWithCachedSizesToArray(brk);

    return true;
}

bool ClientMessage::ParseFromArray(const char *data, int len, const MsgMeta &meta)
{
    /* response failed */
    if (meta.code()) {
        controller_->SetResponseCode(meta.code());
        controller_->SetResponseError(meta.error_text());
        return true;
    }

    /* response message */
    if (!response_->ParseFromArray(data, len)) {
        controller_->SetResponseCode(kErrResponse);
        return false;
    }

    return true;
}

} // namespace qrpc
