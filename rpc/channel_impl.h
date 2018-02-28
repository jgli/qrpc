#ifndef QRPC_RPC_CHANNEL_IMPL_H
#define QRPC_RPC_CHANNEL_IMPL_H

#include <stdint.h>
#include <event.h>
#include <pthread.h>
#include <map>
#include <list>
#include <string>

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include "src/qrpc/util/timer.h"
#include "src/qrpc/util/atomic.h"
#include "src/qrpc/util/compiler.h"
#include "src/qrpc/rpc/builtin.pb.h"

namespace qrpc {

class Channel;
class Compressor;

class Message;
class ClientMessage;

class Connection;
class ClientConnection;

class ChannelImpl : public Channel {
public:
    explicit ChannelImpl(const ChannelOptions &options,
                         const std::string &host, int port,
                         event_base *base);
    virtual ~ChannelImpl();

    virtual int Open();
    virtual int Close();
    virtual int Cancel();

    virtual void CallMethod(const google::protobuf::MethodDescriptor *method,
                            google::protobuf::RpcController *controller,
                            const google::protobuf::Message *request,
                            google::protobuf::Message *response,
                            google::protobuf::Closure *done);

public:
    void SendFail();
    void RecvFail();
    void SendDone(Message *msg);
    bool SendNext(Message **msg); 
    bool RecvDone(const char *payload, int meta, int data);

    /* for self */
    void CancelAllRpc(bool close);
    void CancelRpc(ClientMessage *msg);

    /* for ClientMessage */
    void StartCancel(ClientMessage *msg);
    void OnRpcTimeout(ClientMessage *msg);

    /* for hearbeat */
    void Keepalive();
    void OnKeepaliveDone();

    uint64_t              next_sequence()    { return ++sequence_; }
    ClientConnection*     client_connection(){ return conn_;       }
    event_base*           base()       const { return base_;       }
    int                   port()       const { return port_;       }
    const std::string&    host()       const { return host_;       }
    std::string&          endpoint()         { return endpoint_;   }
    const ChannelOptions& options()    const { return options_;    }
    Compressor*           compressor() const { return compressor_; }

private:
    typedef std::pair<uint64_t, ClientMessage *> MsgItem;
    typedef std::list<std::pair<uint64_t, ClientMessage *> > MsgQueue;

    MsgQueue::iterator find_if(MsgQueue &msgq, uint64_t seq);

    typedef std::pair<uint64_t, Compressor *> LocalComp;
    typedef std::map<pthread_t, LocalComp>::iterator CompIte;

    static Compressor* new_compressor_if_not(pthread_t tid);
    static void del_compressor_if_zero(Compressor *target, pthread_t tid);

private:
    uint64_t sequence_;

    MsgQueue recvq_;
    MsgQueue sendq_;
    MsgItem cur_send_;
    ClientConnection *conn_;

    event_base *base_;
    int port_;
    std::string host_;
    std::string endpoint_;
    pthread_t tid_;
    ChannelOptions options_;

    /* builtin service */
    bool has_status_;
    StatusRequest request_;
    StatusResponse response_;
    BuiltinService::Stub stub_;
    ClientController controller_;
    internal::MethodClosure0<ChannelImpl> closure_;

    /* thread local compressor */
    Compressor *compressor_;
    static pthread_mutex_t mutex_;
    static std::map<pthread_t, LocalComp> compressors_;
};

} // namespace qrpc

#endif /* QRPC_RPC_CHANNEL_IMPL_H */
