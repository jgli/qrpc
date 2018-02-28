#ifndef QRPC_RPC_MESSAGE_H
#define QRPC_RPC_MESSAGE_H

#include <string>
#include <stdint.h>
#include <assert.h>
#include <event.h>

#include <google/protobuf/message.h>
#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include "src/qrpc/util/timer.h"
#include "src/qrpc/util/atomic.h"
#include "src/qrpc/util/compiler.h"
#include "src/qrpc/rpc/message.pb.h"

namespace qrpc {

class Worker;
class Server;
class ServerImpl;

class Channel;
class ChannelImpl;

class Controller;
class ClientController;
class ServerController;

class Connection;
class ClientConnection;
class ServerConnection;

struct MsgHdr {
    int payload_;
    int data_;
    int meta_;
    int compression_;

    MsgHdr() : payload_(0), data_(0), meta_(0), compression_(0) { }
};

/*
 * header is
 * payload size (4 bytes),
 * data size (4 bytes),
 * meta size (2 bytes),
 * compression type (1 byte),
 */
static const int kMsgPayloadSize = 4;
static const int kMsgDataSize = 4;
static const int kMsgMetaSize = 2;
static const int kMsgCompSize = 1;
static const int kMsgHdrSize = 4 + 4 + 2 + 1;

/*
 * The ByteSize() of google protobuf returns 'int',
 * that means the max size of message if INT32_MAX.
 *
 * But the max limited is kDefaultTotalBytesLimit(64MB),
 * refer to 'google/protobuf/io/coded_stream.h'
 */
static const uint32_t kMaxMetaSize = (65535);
static const uint32_t kMaxDataSize = (2147483647 - 65535);
static const uint32_t kMaxPayloadSize = (2147483647);

class Message {
public:
    inline Message() { }
    virtual ~Message();

    virtual int  CompressionType() const = 0;
    virtual void ByteSize(int *smeta, int *sdata) const = 0;
    virtual bool SerializeToArray(char *data, int len) const = 0;
    virtual bool ParseFromArray(const char *data, int len, const MsgMeta &meta) = 0;

private:
    /* No copying allowed */
    Message(const Message &);
    void operator=(const Message &);
};

class ServerMessage : public Message {
public:
    explicit ServerMessage(ServerConnection *conn);
    virtual ~ServerMessage();

    virtual int  CompressionType() const;
    virtual void ByteSize(int *smeta, int *sdata) const;
    virtual bool SerializeToArray(char *data, int len) const;
    virtual bool ParseFromArray(const char *data, int len, const MsgMeta &meta);

public:
    uint64_t id() const { return meta_.sequence(); }
    ServerConnection* server_connection() { return conn_; }

    inline void FinishMethod() { controller_.FinishRequest(); }

    inline void CancelMethod() {
        controller_.CancelRequest();
        meta_.set_code(controller_.code());
        meta_.set_error_text(controller_.error_text());
    }

    inline void CallMethod() {
        service_->CallMethod(method_, &controller_, request_, response_, &closure_);
    }

private:
    void OnRpcDone();

private:
    ServerConnection *conn_;

    MsgMeta meta_;
    int compression_type_;

    google::protobuf::Message *request_;
    google::protobuf::Message *response_;

    google::protobuf::Service *service_;
    const google::protobuf::MethodDescriptor *method_;

    ServerController controller_;
    internal::MethodClosure0<ServerMessage> closure_;
};

class ClientMessage : public Message {
public:
    explicit ClientMessage(ChannelImpl *channel, ClientController *controller,
                           google::protobuf::Closure *done,
                           const google::protobuf::Message *request,
                           google::protobuf::Message *response,
                           const google::protobuf::MethodDescriptor *method);
    virtual ~ClientMessage();

    virtual int  CompressionType() const;
    virtual void ByteSize(int *smeta, int *sdata) const;
    virtual bool SerializeToArray(char *data, int len) const;
    virtual bool ParseFromArray(const char *data, int len, const MsgMeta &meta);

public:
    const MsgMeta& msg_meta() const { return meta_; }
    uint64_t id() const { return meta_.sequence(); }

    void Finish() {
        if (finish_) { return; }
        AssignEndpoints();
        controller_->ResetOwnership();
        done_->Run();
        finish_ = true;
    }
    bool finish() const { return finish_; }

    void StartCancel();
    void SetCancel() { controller_->SetResponseCode(kErrCancel); }

    void NewMonitor();
    void DelMonitor();

private:
    void HandleTimeout();
    void AssignEndpoints();

private:
    MsgMeta meta_;
    bool finish_;

    bool monitor_;
    Timer timer_;

    ChannelImpl *channel_;
    ClientController *controller_;

    google::protobuf::Closure *done_;
    google::protobuf::Message *response_;
    const google::protobuf::Message *request_;
};

} // namespace qrpc

#endif /* QRPC_RPC_MESSAGE_H */
