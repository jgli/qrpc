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

#include <google/protobuf/message.h>
#include <google/protobuf/message_lite.h>
#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include "src/qrpc/util/log.h"
#include "src/qrpc/util/atomic.h"
#include "src/qrpc/util/random.h"
#include "src/qrpc/util/compiler.h"
#include "src/qrpc/util/completion.h"
#include "src/qrpc/util/socket.h"
#include "src/qrpc/rpc/errno.h"
#include "src/qrpc/rpc/closure.h"
#include "src/qrpc/rpc/controller.h"
#include "src/qrpc/rpc/controller_client.h"
#include "src/qrpc/rpc/controller_server.h"
#include "src/qrpc/rpc/message.h"
#include "src/qrpc/rpc/message.pb.h"
#include "src/qrpc/rpc/connection.h"
#include "src/qrpc/rpc/compressor.h"
#include "src/qrpc/rpc/channel.h"
#include "src/qrpc/rpc/channel_impl.h"

using namespace std;
using namespace google::protobuf;

namespace qrpc {

/* protect the shared compressors */
pthread_mutex_t ChannelImpl::mutex_ = PTHREAD_MUTEX_INITIALIZER;

/* the shared compressors */
std::map<pthread_t, ChannelImpl::LocalComp> ChannelImpl::compressors_;

Compressor* ChannelImpl::new_compressor_if_not(pthread_t tid)
{
    Compressor *target = NULL;

    pthread_mutex_lock(&mutex_);

    CompIte ite = compressors_.find(tid);

    if (ite != compressors_.end()) {
        ite->second.first++;
        target = ite->second.second;
    } else {
        Compressor *new_comp = new Compressor();
        if (!new_comp) {
            LOG(FATAL) << "out of memory";
        }

        target = new_comp;
        compressors_.insert(make_pair(tid, make_pair(1, new_comp)));
    }

    pthread_mutex_unlock(&mutex_);

    return target;
}

void ChannelImpl::del_compressor_if_zero(Compressor *source, pthread_t tid)
{
    Compressor *target = NULL;

    pthread_mutex_lock(&mutex_);

    CompIte ite = compressors_.find(tid);

    if (ite != compressors_.end()) {
        assert(ite->second.second == source);
        if (!--ite->second.first) {
            target = source;
            compressors_.erase(ite);
        }
    } else {
        LOG(FATAL) << "invalid local compressor";
    }

    pthread_mutex_unlock(&mutex_);

    /* release local thread compressor */
    if (target) { delete target; }
}

ChannelImpl::ChannelImpl(const ChannelOptions &options,
                         const string &host, int port,
                         event_base *base)
    : sequence_(0)
    , conn_(NULL)
    , base_(base)
    , port_(port)
    , host_(host)
    , tid_(pthread_self())
    , options_(options)
    , has_status_(false)
    , stub_(this)
    , controller_(ControllerOptions())
    , closure_(this, &ChannelImpl::OnKeepaliveDone, false)
    , compressor_(new_compressor_if_not(tid_))
{
    char tmp[1024] = { 0 };

    if (!gethostname(tmp, 1024)) {
        endpoint_ = tmp;
    } else {
        endpoint_ = "local";
        LOG(ERROR) << "gethostname failed: " << errno;
    }
}

ChannelImpl::~ChannelImpl()
{
    CancelAllRpc(true);

    delete conn_;
    conn_ = NULL;

    /* release compressor */
    del_compressor_if_zero(compressor_, tid_);
}

int ChannelImpl::Open()
{
    if (pthread_self() != tid_) {
        LOG(ERROR) << "run in the alloc thread context";
        return kErrCtx;
    }
    
    if (conn_) {
        LOG(ERROR) << "the channel has beed opened";
        return kError;
    }

    conn_ = new ClientConnection(this);
    if (!conn_) {
        LOG(ERROR) << "open channel failed";
        return kErrMem;
    }

    return kOk;
}

int ChannelImpl::Close()
{
    if (pthread_self() != tid_) {
        LOG(ERROR) << "run in the alloc thread context";
        return kErrCtx;
    }

    CancelAllRpc(true);

    delete conn_;
    conn_ = NULL;

    return 0;
}

int ChannelImpl::Cancel()
{
    if (pthread_self() != tid_) {
        LOG(ERROR) << "run in the alloc thread context";
        return kErrCtx;
    }

    CancelAllRpc(false);

    return 0;
}

void ChannelImpl::Keepalive()
{
    if (!sendq_.empty()) {
        return;
    }
    if (!recvq_.empty()) {
        return;
    }

    if (has_status_) {
        return;
    }

    has_status_ = true;
    controller_.Reset();
    stub_.Status(&controller_, &request_, &response_, &closure_);
}

void ChannelImpl::OnKeepaliveDone()
{
    if (controller_.Failed()) {
        LOG(ERROR) << "remote server "
            << host_
            << ":"
            << port_
            << " is offline";
    } else {
        DLOG(INFO) << "remote server "
            << host_
            << ":"
            << port_
            << " is online";
    }

    has_status_ = false;
}

void ChannelImpl::CallMethod(const google::protobuf::MethodDescriptor *method,
                             google::protobuf::RpcController *controller,
                             const google::protobuf::Message *request,
                             google::protobuf::Message *response,
                             google::protobuf::Closure *done)
{
    if (unlikely(!request)) {
        LOG(FATAL) << "rpc request is null";
    }
    if (unlikely(!response)) {
        LOG(FATAL) << "rpc response is null";
    }
    if (unlikely(!done)) {
        LOG(FATAL) << "rpc callback is null";
    }
    if (unlikely(!controller)) {
        LOG(FATAL) << "rpc controller is null";
    }

    ClientController *ctl = (ClientController *)controller;

    //if (!request->IsInitialized()) {
    //    ctl->SetResponseCode(kErrField);
    //    ctl->SetResponseError(rerror(kErrField));
    //    return done->Run();
    //}

    ClientMessage *cli_msg = new ClientMessage(this, ctl, done,
            request, response, method);
    if (!cli_msg) {
        LOG(FATAL) << "alloc client message failed!!!";
    }

    if (sendq_.empty()) {
        conn_->EnableUpload();
    }
    sendq_.push_back(MsgItem(cli_msg->id(), cli_msg));

    cli_msg->NewMonitor();
}

list<pair<uint64_t, ClientMessage *> >::iterator
__always_inline ChannelImpl::find_if(MsgQueue &msgq, uint64_t seq)
{
    for (MsgQueue::iterator it = msgq.begin();
         it != msgq.end(); ++it) {
        if (it->first == seq) {
            return it;
        }
    }
    
    return msgq.end();
}

inline void ChannelImpl::CancelRpc(ClientMessage *msg)
{
    /* set cancel flag */
    msg->SetCancel();

    /* cancel watcher */
    msg->DelMonitor();

    /* 
     * It's safe to notify user immediately,
     * refer to Connection::Encode().
     */
    msg->Finish();

    delete msg;
}

void ChannelImpl::CancelAllRpc(bool close)
{
    MsgQueue::iterator it;

    for (it = recvq_.begin(); it != recvq_.end(); ++it) {
        CancelRpc(it->second);
    }

    if (cur_send_.second) {
        sendq_.pop_front();

        cur_send_.second->SetCancel();
        cur_send_.second->DelMonitor();
        cur_send_.second->Finish();

        if (close) {
            delete cur_send_.second;
            cur_send_.first = 0;
            cur_send_.second = NULL;
        }
    }

    for (it = sendq_.begin(); it != sendq_.end(); ++it) {
        CancelRpc(it->second);
    }

    recvq_.clear();
    sendq_.clear();

    if (cur_send_.second) {
        sendq_.push_back(cur_send_);
    }
}

void ChannelImpl::StartCancel(ClientMessage *msg)
{
    /* cancel watcher */
    msg->DelMonitor();

    bool free = true;
    MsgQueue::iterator it;
    
    /* in recv queue */
    it = find_if(recvq_, msg->id());
    if (it != recvq_.end()) {
        recvq_.erase(it);
        goto notify;
    }

    /* the message is in process */
    if (cur_send_.second == msg) {
        free = false;
        goto notify;
    }

    /* in send queue */
    it = find_if(sendq_, msg->id());
    if (it != recvq_.end()) {
        sendq_.erase(it);
    } else {
        /* couldn't be here */
        LOG(FATAL) << "invalid message";
    }

notify:
    /* 
     * It's safe to notify user immediately,
     * refer to Connection::Encode().
     */
    msg->Finish();
    if (free) { delete msg; }
}

void ChannelImpl::OnRpcTimeout(ClientMessage *msg)
{
    bool free = true;
    MsgQueue::iterator it;
    
    /* in recv queue */
    it = find_if(recvq_, msg->id());
    if (it != recvq_.end()) {
        recvq_.erase(it);
        goto notify;
    }

    /* the message is in process */
    if (cur_send_.second == msg) {
        free = false;
        goto notify;
    }

    /* in send queue */
    it = find_if(sendq_, msg->id());
    if (it != recvq_.end()) {
        sendq_.erase(it);
    } else {
        /* couldn't be here */
        LOG(FATAL) << "invalid message";
    }

notify:
    /* 
     * It's safe to notify user immediately,
     * refer to Connection::Encode().
     */
    msg->Finish();
    if (free) { delete msg; }
}

void ChannelImpl::RecvFail()
{
    ClientMessage *cli_msg = cur_send_.second;
    if (cli_msg && cli_msg->finish()) {
       sendq_.pop_front(); 
       delete cli_msg;
    }

    delete conn_;
    conn_ = NULL;
    cur_send_.first = 0;
    cur_send_.second = NULL;

    /* retransmit the finish-send requests */
    for (MsgQueue::reverse_iterator rit = recvq_.rbegin();
         rit != recvq_.rend();
         ++rit) {
        sendq_.push_front(*rit);
    }
    recvq_.clear();

    conn_ = new ClientConnection(this);
    if (!conn_) {
        LOG(FATAL) << "alloc channel object failed";
    }
}

void ChannelImpl::SendFail()
{
    ClientMessage *cli_msg = cur_send_.second;
    if (cli_msg && cli_msg->finish()) {
       sendq_.pop_front(); 
       delete cli_msg;
    }

    delete conn_;
    conn_ = NULL;
    cur_send_.first = 0;
    cur_send_.second = NULL;

    /* retransmit the finish-send requests */
    for (MsgQueue::reverse_iterator rit = recvq_.rbegin();
         rit != recvq_.rend();
         ++rit) {
        sendq_.push_front(*rit);
    }
    recvq_.clear();

    conn_ = new ClientConnection(this);
    if (!conn_) {
        LOG(FATAL) << "alloc channel object failed";
    }
}

bool ChannelImpl::SendNext(Message **msg)
{
    assert(cur_send_.second == NULL);

    if (sendq_.empty()) {
        return false;
    }

    cur_send_ = sendq_.front();
    *msg = cur_send_.second;

    return true;
}

void ChannelImpl::SendDone(Message *msg)
{
    ClientMessage *cli_msg = (ClientMessage *)msg;

    assert(cur_send_.second == msg);

    if (cli_msg->finish()) {
        sendq_.pop_front();
        delete cli_msg;
    } else {
        sendq_.pop_front();
        recvq_.push_back(cur_send_);
    }

    cur_send_.first = 0;
    cur_send_.second = NULL;
}

bool ChannelImpl::RecvDone(const char *payload, int meta, int data)
{
    MsgMeta msg_meta;

    bool rc = msg_meta.ParseFromArray(payload, meta);
    if (!rc) {
        LOG(ERROR) << "parse MsgMeta failed!!!";
        return false;
    }

    MsgQueue::iterator it = find_if(recvq_, msg_meta.sequence());
    if (it == recvq_.end()) {
        LOG(WARNING) << "find canceled rpc"
            << ", from: "
            << host_
            << ":"
            << port_
            << ", sequence: "
            << msg_meta.sequence();
        return true;
    }

    ClientMessage *cli_msg = it->second;
    recvq_.erase(it);

    /* cancel watcher */
    cli_msg->DelMonitor();

    rc = cli_msg->ParseFromArray(payload + meta, data, msg_meta);
    if (!rc) {
        LOG(ERROR) << "parse response message failed!!!";
    }

    cli_msg->Finish();
    delete cli_msg;

    return rc;
}

} // namespace qrpc
