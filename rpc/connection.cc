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
#include <time.h>
#include <string>

#include "src/qrpc/util/log.h"
#include "src/qrpc/util/timer.h"
#include "src/qrpc/util/socket.h"
#include "src/qrpc/util/compiler.h"
#include "src/qrpc/rpc/errno.h"
#include "src/qrpc/rpc/closure.h"
#include "src/qrpc/rpc/controller.h"
#include "src/qrpc/rpc/controller_client.h"
#include "src/qrpc/rpc/controller_server.h"
#include "src/qrpc/rpc/message.h"
#include "src/qrpc/rpc/message.pb.h"
#include "src/qrpc/rpc/channel.h"
#include "src/qrpc/rpc/channel_impl.h"
#include "src/qrpc/rpc/builtin.h"
#include "src/qrpc/rpc/server.h"
#include "src/qrpc/rpc/server_impl.h"
#include "src/qrpc/rpc/worker.h"
#include "src/qrpc/rpc/compressor.h"
#include "src/qrpc/rpc/connection.h"

using namespace std;

namespace qrpc {

static const bool kUseClock = false;

// -------------------------------------------------------------
// class Connection
// -------------------------------------------------------------

Connection::Connection()
    : sfd_(-1)
    , rstate_(kRead)
    , wstate_(kWrite)
    , rsize_(0)
    , rbytes_(0)
    , rcur_(NULL)
    , rbuf_(NULL)
    , rmsg_(NULL)
    , wmsg_(NULL)
    , wsize_(0)
    , wbytes_(0)
    , wcur_(NULL)
    , wbuf_(NULL)
    , msg_body_(NULL)
    , hdr_comp_(NULL)
    , hdr_meta_(NULL)
    , hdr_data_(NULL)
    , hdr_payload_(NULL)
    , compressor_(NULL)
{

}

Connection::~Connection()
{
    free(rbuf_);
    free(wbuf_);

    assert(sfd_ = -1);
}

void __always_inline Connection::EnableUpload()
{
    short events = EV_READ | EV_WRITE | EV_PERSIST;

    if (events == event_get_events(&event_)) {
        return;
    }

    event_base *base = event_get_base(&event_);

    assert(base != NULL);

    if (event_del(&event_)) {
        LOG(FATAL) << "delete event failed!!!";
    }
    if (event_assign(&event_, base, sfd_,
                     EV_READ | EV_WRITE | EV_PERSIST,
                     HandleConnectedEvent, this)) {
        LOG(FATAL) << "set event failed!!!";
    }
    if (event_add(&event_, 0)) {
        LOG(FATAL) << "add event failed!!!";
    }
}

void __always_inline Connection::DisableUpload()
{
    short events = EV_READ | EV_PERSIST;

    if (events == event_get_events(&event_)) {
        return;
    }

    event_base *base = event_get_base(&event_);

    assert(base != NULL);

    if (event_del(&event_)) {
        LOG(FATAL) << "delete event failed!!!";
    }
    if (event_assign(&event_, base, sfd_,
                     EV_READ | EV_PERSIST,
                     HandleConnectedEvent, this)) {
        LOG(FATAL) << "set event failed!!!";
    }
    if (event_add(&event_, 0)) {
        LOG(FATAL) << "add event failed!!!";
    }
}

bool Connection::ExpandWbuf(size_t required)
{
    do {
        wsize_ *= 2;
    } while ((size_t)wsize_ < required);

    char *nbuf = (char *)realloc(wbuf_, wsize_);
    if (!nbuf) {
        LOG(ERROR) << "realloc write buf failed!!!";
        return false;
    }

    hdr_payload_ = wbuf_ = nbuf;
    hdr_data_ = wbuf_ + kMsgPayloadSize;
    hdr_meta_ = wbuf_ + kMsgPayloadSize + kMsgDataSize;
    hdr_comp_ = wbuf_ + kMsgPayloadSize + kMsgDataSize + kMsgMetaSize;
    msg_body_ = wbuf_ + kMsgHdrSize;

    return true;
}

Connection::Status Connection::Encode()
{
    struct NetHeader {
        uint32_t payload;
        uint32_t data;
        uint16_t meta;
        uint8_t  comp;
    };

    NetHeader net_hdr;

    bool compress = false;
    int comp = wmsg_->CompressionType();

    int meta, data;
    wmsg_->ByteSize(&meta, &data);

    int payload = meta + data;
    int required = payload + kMsgHdrSize;

    if (comp != kNoCompression) {
        compress = true;
    }
    if (payload < kCompressionThreshold) {
        comp = kNoCompression;
        compress = false;
    }

    if (!compress) {
        if (required > wsize_ && !ExpandWbuf(required)) {
            return kEncodeError;
        }

        if (!wmsg_->SerializeToArray(msg_body_, payload)) {
            LOG(ERROR) << "serialize message failed!!!";
            return kEncodeError;
        }
    } else {
        compressor_->UseCompression((CompressionType)comp);

        char *temp = compressor_->ExpandBufferCache(payload);
        if (!wmsg_->SerializeToArray(temp, payload)) {
            LOG(ERROR) << "serialize message failed!!!";
            return kEncodeError;
        }

compress:
        size_t rlen = wsize_ - kMsgHdrSize;
        int rc = compressor_->Compress(temp, payload, msg_body_, rlen, &rlen);

        switch (rc) {
        case kCompOk:
            payload = rlen;
            required = payload + kMsgHdrSize;
            break;
        case kCompBufferTooSmall:
            if (ExpandWbuf(wsize_ * 2)) {
                goto compress;
            } else {
                return kEncodeError;
            }
            break;
        case kCompInvalidInput:
            LOG(FATAL) << "invalid input message for compression: " << comp;
            break;
        }
    }

    net_hdr.comp = comp;
    net_hdr.meta = htons(meta);
    net_hdr.data = htonl(data);
    net_hdr.payload = htonl(payload);

    memcpy(hdr_comp_, &net_hdr.comp, kMsgCompSize);
    memcpy(hdr_meta_, &net_hdr.meta, kMsgMetaSize);
    memcpy(hdr_data_, &net_hdr.data, kMsgDataSize);
    memcpy(hdr_payload_, &net_hdr.payload, kMsgPayloadSize);

    wcur_ = wbuf_;
    wbytes_ = required;

    return kEncodeOk;
}

Connection::Status Connection::Decode()
{
    struct NetHeader {
        uint32_t payload;
        uint32_t data;
        uint16_t meta;
        uint8_t  comp;
    } __attribute__((aligned(1)));

    NetHeader *net_hdr = (NetHeader *)rcur_;

header:
    if (rmsg_hdr_.payload_) {
        goto payload;
    }

    if (rbytes_ < kMsgHdrSize) {
        return kDecodeFragment;
    }

    rmsg_hdr_.compression_ = net_hdr->comp;
    rmsg_hdr_.meta_ = ntohs(net_hdr->meta);
    rmsg_hdr_.data_ = ntohl(net_hdr->data);
    rmsg_hdr_.payload_ = ntohl(net_hdr->payload);

    rcur_ += kMsgHdrSize;
    rbytes_ -= kMsgHdrSize;

payload:
    if (rbytes_ < rmsg_hdr_.payload_) {
        return kDecodeFragment;
    }

    if (rmsg_hdr_.compression_ == kNoCompression) {
        rmsg_ = rcur_;
    } else {
        CompressionType type = (CompressionType)rmsg_hdr_.compression_;
        compressor_->UseCompression(type);

        size_t required = rmsg_hdr_.meta_ + rmsg_hdr_.data_;
        char *temp = compressor_->ExpandBufferCache(required);

        size_t ilen = rmsg_hdr_.payload_;
        size_t rlen = 0;
        int rc = compressor_->Uncompress(rcur_, ilen, temp, required, &rlen);

        switch (rc) {
        case kCompOk:
            assert(required == rlen);
            rmsg_ = temp;
            break;
        case kCompBufferTooSmall:
            LOG(FATAL) << "corrupt message header: " << type;
            break;
        case kCompInvalidInput:
            LOG(FATAL) << "corrupt message body: " << type;
            break;
        }
    }

    rcur_ += rmsg_hdr_.payload_;
    rbytes_ -= rmsg_hdr_.payload_;

    return kDecodeOk;

    /* avoid gcc warning */
    if (0) goto header;
}

/*
 * read from network as much as we can, handle buffer overflow and connection
 * close.
 * before reading, move the remaining incomplete fragment of a command
 * (if any) to the beginning of the buffer.
 *
 * To protect us from someone flooding a connection with bogus data causing
 * the connection to eat up all available memory, break out and start looking
 * at the data I've got after a number of reallocs...
 *
 * @return enum Status
 */
Connection::Status Connection::Recv()
{
    Status status = kRecvAgain;
    int res, avail;
    int num_allocs = 0;

    if (rcur_ != rbuf_) {
        if (rbytes_) {
            memmove(rbuf_, rcur_, rbytes_);
        } else {
            /* there's nothing to copy */
        }
        rcur_ = rbuf_;
    }

read_much:
    if (rbytes_ >= rsize_) {
        if (num_allocs++ == 4) {
            goto out;
        }

        char *nbuf = (char *)realloc(rbuf_, rsize_ * 2);
        if (!nbuf) {
            status = kRecvError;
            LOG(ERROR) << "realloc read buf failed!!!";
            goto out;
        }

        rsize_ *= 2;
        rcur_ = rbuf_ = nbuf;
    }

    avail = rsize_ - rbytes_;
    res = recv(sfd_, rbuf_ + rbytes_, avail, 0);

    if (res > 0) {
        rbytes_ += res;
        status = kRecvOk;
        if (res == avail) {
            goto read_much;
        } else {
            goto out;
        }
    } else if (res == 0) {
        status = kRecvError;
    } else {
        if (errno == EINTR) {
            goto read_much;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            goto out;
        } else {
            status = kRecvError;
            DLOG(ERROR) << "recv msg failed: " << strerror(errno);
        }
    }

out:
    return status;
}

bool Connection::OnRecv()
{
    bool result = true;

    for (; ;) {
        if (rstate_ == kRead) {
            switch (Recv()) {
            case kRecvOk:
                rstate_ = kParse;
                break;
            case kRecvAgain:
                rstate_ = kWait;
                break;
            case kRecvError:
                rstate_ = kClose;
                break;
            default:
                rstate_ = kClose;
                LOG(FATAL) << "fatal branch!!!";
                break;
            }
        } else if (rstate_ == kParse) {
            switch (Decode()) {
            case kDecodeOk:
                RecvDone(rmsg_, rmsg_hdr_.meta_, rmsg_hdr_.data_);
                memset(&rmsg_hdr_, 0, sizeof(rmsg_hdr_));
                rmsg_ = NULL;
                rstate_ = kParse;
                break;
            case kDecodeFragment:
                rstate_ = kWait;
                break;
            case kDecodeError:
                rstate_ = kClose;
                break;
            default:
                rstate_ = kClose;
                LOG(FATAL) << "fatal branch!!!";
                break;
            }
        } else if (rstate_ == kWait) {
            rstate_ = kRead;
            break;
        } else if (rstate_ == kClose) {
            result = false;
            break;
        } else {
            result = false;
            LOG(FATAL) << "fatal branch!!!";
            break;
        }
    }

    return result;
}

Connection::Status Connection::Send()
{
    int res = 0;

    if (wmsg_) {
        goto send;
    }

peek:
    if (!SendNext(&wmsg_)) {
        return kSendNothing;
    }

    switch (Encode()) {
    case kEncodeOk:
        break;
    case kEncodeError:
        return kSendError;
    default:
        LOG(FATAL) << "ill branch!!!";
    }

send:
    res = send(sfd_, wcur_, wbytes_, MSG_NOSIGNAL);

    if (res > 0) {
        wcur_ += res;
        wbytes_ -= res;
        if (wbytes_ == 0) {
            return kSendOk;
        } else {
            return kSendAgain;
        }
    } else if (res == 0) {
        return kSendAgain;
    } else {
        if (errno == EINTR) {
            goto send;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return kSendAgain;
        } else {
            DLOG(ERROR) << "send msg failed: " << strerror(errno);
            return kSendError;
        }
    }

    /* avoid gcc warning */
    if (0) goto peek;
}

bool Connection::OnSend()
{
    bool result = true;

    for (; ;) {
        if (wstate_ == kWrite) {
            switch (Send()) {
            case kSendOk:
                SendDone(wmsg_);
                wmsg_ = NULL;
                wstate_ = kWrite;
                break;
            case kSendAgain:
                wstate_ = kWait;
                break;
            case kSendError:
                wstate_ = kClose;
                break;
            case kSendNothing:
                DisableUpload();
                wstate_ = kWait;
                break;
            default:
                wstate_ = kClose;
                LOG(FATAL) << "fatal branch!!!";
                break;
            }

        } else if (wstate_ == kWait) {
            wstate_ = kWrite;
            break;

        } else if (wstate_ == kClose) {
            result = false;
            break;

        } else {
            result = false;
            LOG(FATAL) << "fatal branch!!!";
            break;
        }
    }

    return result;
}

void Connection::HandleConnectedEvent(int fd, short flags, void *arg)
{
    Connection *me = (Connection *)arg;

    if ((flags & EV_READ) && !me->OnRecv()) {
        DLOG(ERROR) << "recv message failed!!!";
        me->RecvFail();
        return;
    }

    if ((flags & EV_WRITE) && !me->OnSend()) {
        DLOG(ERROR) << "send message failed!!!";
        me->SendFail();
        return;
    }
}

// -------------------------------------------------------------
// class ServerConnection
// -------------------------------------------------------------

ServerConnection::ServerConnection(Worker *worker, int sfd,
        std::string &local_addr, std::string &remote_addr)
    : worker_(worker)
    , update_(0)
    , has_timer_(false)
    , use_clock_(kUseClock)
    , connected_(true)
    , local_addr_(local_addr)
    , remote_addr_(remote_addr)
{
    const ServerOptions &options = worker->server_impl()->options();

    rsize_ = options.min_rbuf_size;
    if (rsize_ < kMsgHdrSize) {
        rsize_ = kMsgHdrSize;
    }
    rbuf_ = (char *)malloc(rsize_);
    if (!rbuf_) {
        LOG(FATAL) << "alloc read buf failed!!!";
    }
    rcur_ = rbuf_;

    wsize_ = options.min_sbuf_size;
    if (wsize_ < kMsgHdrSize) {
        wsize_ = kMsgHdrSize;
    }
    wbuf_ = (char *)malloc(wsize_);
    if (!wbuf_) {
        LOG(FATAL) << "alloc write buf failed!!!";
    }
    hdr_payload_ = wbuf_;
    hdr_data_ = wbuf_ + kMsgPayloadSize;
    hdr_meta_ = wbuf_ + kMsgPayloadSize + kMsgDataSize;
    hdr_comp_ = wbuf_ + kMsgPayloadSize + kMsgDataSize + kMsgMetaSize;
    msg_body_ = wbuf_ + kMsgHdrSize;

    compressor_ = worker->compressor();

    sfd_ = sfd;
    if (event_assign(&event_, worker->base(), sfd,
                     EV_READ | EV_PERSIST,
                     HandleConnectedEvent, this)) {
        LOG(FATAL) << "set event failed!!!";
    }
    if (event_add(&event_, 0)) {
        LOG(FATAL) << "add event failed!!!";
    }

    if (!use_clock_) {
        NewTimerKeepalive();
    } else {
        update_ = CurClock();
        NewClockKeepalive();
    }
}

ServerConnection::~ServerConnection()
{
    assert(has_timer_ == false);
    assert(recvq_.empty() == true);
    assert(sendq_.empty() == true);
}

void ServerConnection::CloseConnection()
{
    if (!connected_) {
        return;
    }

    if (!use_clock_) {
        DelTimerKeepalive();
    } else {
        DelClockKeepalive();
    }

    event_del(&event_);
    close(sfd_);
    sfd_ = -1;
    connected_ = false;

    for (MsgQueue::iterator it = sendq_.begin();
         it != sendq_.end(); ++it) {
        ServerMessage *msg;

        /* TODO: notify user */
        msg = it->second;
        msg->FinishMethod();
        delete msg;
    }

    sendq_.clear();
    cur_send_.first = 0;
    cur_send_.second = NULL;

    /* TODO: cancel recvq_ */
}

void __always_inline
ServerConnection::ReleaseConnection()
{
    assert(sendq_.empty() == true);

    if (!recvq_.empty()) {
        return;
    }

    worker_->Unlink(this);
}

void ServerConnection::Close()
{
    CloseConnection();
    ReleaseConnection();
}

void ServerConnection::Send(ServerMessage *msg)
{
    OnRpcResponse(msg);
}

void ServerConnection::HandleClockKeepalive()
{
    has_timer_ = false;
    
    ServerImpl *srv_impl = worker_->server_impl();
    const ServerOptions &options = srv_impl->options();

    uint64_t diff = CurClock() - update_;
    uint64_t timeout = options.keep_alive_time * 1000;

    if (diff < timeout) {
        NewClockKeepalive();
    } else {
        CloseConnection();
        ReleaseConnection();
    }
}

uint64_t __always_inline ServerConnection::CurClock()
{
    struct timespec cur;

    if (clock_gettime(CLOCK_MONOTONIC, &cur)) {
        LOG(FATAL) << "clock_gettime failed: " << strerror(errno);;
    }

    return cur.tv_sec;
}

inline void ServerConnection::DelClockKeepalive()
{
    if (!has_timer_) {
        return;
    }

    has_timer_ = false;
    timer_.SchedCancel();
}

inline void ServerConnection::NewClockKeepalive()
{
    assert(has_timer_ == false);
    has_timer_ = true;

    ServerImpl *srv_impl = worker_->server_impl();
    const ServerOptions &options = srv_impl->options();

    uint64_t timeout = options.keep_alive_time * 1000;

    uint64_t diff = CurClock() - update_;
    if (unlikely(diff >= timeout)) {
        diff = timeout - 1;
    }

    timeout -= diff;

    timer_.Set(worker_->base(), timeout,
               tr1::bind(&ServerConnection::HandleClockKeepalive, this));
    timer_.SchedOneshot();
}

void ServerConnection::HandleTimerKeepalive()
{
    has_timer_ = false;
    CloseConnection();
    ReleaseConnection();
}

inline void ServerConnection::DelTimerKeepalive()
{
    if (!has_timer_) {
        return;
    }

    has_timer_ = false;
    timer_.SchedCancel();
}

inline void ServerConnection::UpdTimerKeepalive()
{
    if (has_timer_) {
        timer_.SchedCancel();
    }

    has_timer_ = true;

    ServerImpl *srv_impl = worker_->server_impl();
    const ServerOptions &options = srv_impl->options();

    uint64_t timeout = options.keep_alive_time * 1000;

    timer_.Set(worker_->base(), timeout,
               tr1::bind(&ServerConnection::HandleTimerKeepalive, this));
    timer_.SchedOneshot();
}

inline void ServerConnection::NewTimerKeepalive()
{
    assert(has_timer_ == false);
    has_timer_ = true;

    ServerImpl *srv_impl = worker_->server_impl();
    const ServerOptions &options = srv_impl->options();

    uint64_t timeout = options.keep_alive_time * 1000;

    timer_.Set(worker_->base(), timeout,
               tr1::bind(&ServerConnection::HandleTimerKeepalive, this));
    timer_.SchedOneshot();
}

list<pair<uint64_t, ServerMessage *> >::iterator
__always_inline ServerConnection::find_if(MsgQueue &msgq, uint64_t seq)
{
    for (MsgQueue::iterator it = msgq.begin();
         it != msgq.end(); ++it) {
        if (it->first == seq) {
            return it;
        }
    }
    
    return msgq.end();
}

void __always_inline
ServerConnection::OnRpcCancel(const MsgMeta &meta)
{
    MsgQueue::iterator it;

    /* FIXME: Connection::Encode() */
    if (cur_send_.second->id() == meta.sequence()) {
        return;
    }

    it = find_if(sendq_, meta.sequence());
    if (it != sendq_.end()) {
        it->second->CancelMethod();
        return;
    }

    it = find_if(recvq_, meta.sequence());
    if (it != recvq_.end()) {
        it->second->CancelMethod();
        return;
    }

    DLOG(INFO) << "find a delayed cancel request RPC";
}

void __always_inline
ServerConnection::OnRpcRequest(ServerMessage *msg)
{
    recvq_.push_back(MsgItem(msg->id(), msg));

    msg->CallMethod();

    if (!use_clock_) {
        UpdTimerKeepalive();
    } else {
        update_ = CurClock();
    }
}

void __always_inline
ServerConnection::OnRpcResponse(ServerMessage *msg)
{
    MsgQueue::iterator it;

    it = find_if(recvq_, msg->id());
    if (it != recvq_.end()) {
        recvq_.erase(it);
    } else {
        LOG(FATAL) << "invalid message!!!";
    }

    if (!connected_) {
        /* TODO: notify user */
        msg->FinishMethod();
        delete msg;
        ReleaseConnection();
        return;
    }

    if (sendq_.empty()) {
        Connection::EnableUpload();
    }
    sendq_.push_back(MsgItem(msg->id(), msg));
}

void __always_inline
ServerConnection::OnRpcFinish(ServerMessage *msg)
{
    MsgQueue::iterator it;

    it = find_if(sendq_, msg->id());
    if (it != sendq_.end()) {
        sendq_.erase(it);
    } else {
        LOG(FATAL) << "invalid message!!!";
    }

    msg->FinishMethod();

    delete msg;

    if (!use_clock_) {
        UpdTimerKeepalive();
    } else {
        update_ = CurClock();
    }
}

void ServerConnection::SendFail()
{
    CloseConnection();
    ReleaseConnection();
}

void ServerConnection::RecvFail()
{
    CloseConnection();
    ReleaseConnection();
}

bool ServerConnection::SendNext(Message **msg)
{
    if (sendq_.empty()) {
        return false;
    }

    cur_send_ = sendq_.front();
    *msg = cur_send_.second;

    return true;
}

void ServerConnection::SendDone(Message *msg)
{
    ServerMessage *srv_msg = (ServerMessage *)msg;

    assert(cur_send_.second == srv_msg);

    OnRpcFinish(srv_msg);
    cur_send_.first = 0;
    cur_send_.second = NULL;
}

bool ServerConnection::RecvDone(const char *payload, int meta, int data)
{
    MsgMeta msg_meta;

    bool rc = msg_meta.ParseFromArray(payload, meta);
    if (!rc) {
        LOG(ERROR) << "parse MsgMeta failed!!!";
        return false;
    }

    if (msg_meta.cancel()) {
        assert(data == 0);
        OnRpcCancel(msg_meta);
        return true;
    }

    ServerMessage *msg = new ServerMessage(this);
    if (!msg) {
        LOG(FATAL) << "alloc server message failed!!!";
    }

    rc = msg->ParseFromArray(payload + meta, data, msg_meta);
    if (rc) {
        OnRpcRequest(msg);
    } else {
        LOG(ERROR) << "parse request message failed!!!";
        delete msg;
        return false;
    }

    return rc;
}

// -------------------------------------------------------------
// class ClientConnection
// -------------------------------------------------------------

ClientConnection::ClientConnection(ChannelImpl *channel)
    : channel_(channel)
    , connected_(false)
    , connecting_(false)
    , has_timer_(false)
{
    const ChannelOptions &options = channel->options();

    rsize_ = options.min_rbuf_size;
    if (rsize_ < kMsgHdrSize) {
        rsize_ = kMsgHdrSize;
    }
    rbuf_ = (char *)malloc(rsize_);
    if (!rbuf_) {
        LOG(FATAL) << "alloc read buf failed!!!";
    }
    rcur_ = rbuf_;

    wsize_ = options.min_sbuf_size;
    if (wsize_ < kMsgHdrSize) {
        wsize_ = kMsgHdrSize;
    }
    wbuf_ = (char *)malloc(wsize_);
    if (!wbuf_) {
        LOG(FATAL) << "alloc write buf failed!!!";
    }
    hdr_payload_ = wbuf_;
    hdr_data_ = wbuf_ + kMsgPayloadSize;
    hdr_meta_ = wbuf_ + kMsgPayloadSize + kMsgDataSize;
    hdr_comp_ = wbuf_ + kMsgPayloadSize + kMsgDataSize + kMsgMetaSize;
    msg_body_ = wbuf_ + kMsgHdrSize;

    compressor_ = channel->compressor();

    Connect();
}

ClientConnection::~ClientConnection()
{
    DelTimer();

    if (!connected_ && !connecting_) {
        return;
    }

    event_del(&event_);
    close(sfd_);
    sfd_ = -1;
}

void __always_inline ClientConnection::DelTimer()
{
    if (!has_timer_) {
        return;
    }

    has_timer_ = false;
    timer_.SchedCancel();
}

void ClientConnection::HandleIdleEvent()
{
    assert(connected_ == false);
    assert(connecting_ == false);

    has_timer_ = false;
    Connect();
}

inline void ClientConnection::DelIdle()
{
    DelTimer();
}

inline void ClientConnection::NewIdle()
{
    assert(has_timer_ == false);

    has_timer_ = true;

    const ChannelOptions &options = channel_->options();

    timer_.Set(channel_->base(), options.retry_interval,
               tr1::bind(&ClientConnection::HandleIdleEvent, this));
    timer_.SchedOneshot();
}

void ClientConnection::HandleWatchEvent()
{
    assert(connected_ == false);
    assert(connecting_ == true);

    /* connecting timeout */
    LOG(WARNING) << "connect to "
                 << channel_->host()
                 << ":"
                 << channel_->port()
                 << " timeout";

    if (event_del(&event_)) {
        LOG(FATAL) << "remove event failed!!!";
    }

    close(sfd_);
    sfd_ = -1;
    connecting_ = false;

    has_timer_ = false;
    Connect();
}

inline void ClientConnection::DelWatcher()
{
    DelTimer();
}

inline void ClientConnection::NewWatcher()
{
    assert(has_timer_ == false);

    has_timer_ = true;

    const ChannelOptions &options = channel_->options();

    timer_.Set(channel_->base(), options.connect_timeout,
               tr1::bind(&ClientConnection::HandleWatchEvent, this));
    timer_.SchedOneshot();
}

void ClientConnection::HandleHeartbeatEvent()
{
    assert(connected_ == true);
    assert(connecting_ == false);

    has_timer_ = false;
    channel_->Keepalive();
    NewHeartbeat();
}

inline void ClientConnection::DelHeartbeat()
{
    DelTimer();
}

inline void ClientConnection::NewHeartbeat()
{
    assert(has_timer_ == false);

    has_timer_ = true;

    const ChannelOptions &options = channel_->options();

    timer_.Set(channel_->base(), options.heartbeat_interval,
               tr1::bind(&ClientConnection::HandleHeartbeatEvent, this));
    timer_.SchedOneshot();
}

void ClientConnection::Connected()
{
    assert(connected_ == false);
    assert(connecting_ == false);

    connecting_ = false;
    connected_ = true;

    if (event_assign(&event_, channel_->base(), sfd_,
                     EV_READ | EV_WRITE | EV_PERSIST,
                     HandleConnectedEvent, this)) {
        LOG(FATAL) << "set event failed!!!";
    }
    if (event_add(&event_, 0)) {
        LOG(FATAL) << "add event failed!!!";
    }

    if (channel_->options().heartbeat_interval) {
        NewHeartbeat();
        DLOG(INFO) << "start the RPC hearbeat";
    } else {
        DLOG(INFO) << "disable the RPC heartbeat";
    }

    local_addr_ = unresolve_desc(sfd_);
    remote_addr_ = unresolve_peer_desc(sfd_);
}

void ClientConnection::HandleConnectingEvent(int fd, short flags, void *arg)
{
    ClientConnection *me = (ClientConnection *)arg;

    assert(me->connected_ == false);
    assert(me->connecting_ == true);

    me->DelWatcher();

    if (!(flags & EV_WRITE)) {
        LOG(ERROR) << "fatal event!!!";
        close(me->sfd_);
        me->sfd_ = -1;
        me->connecting_= false;
        me->NewIdle();
        return;
    }

    int status = get_soerror(me->sfd_);
    if (status || errno) {
        LOG(ERROR) << "connect to "
            << me->channel_->host()
            << ":"
            << me->channel_->port()
            << " failed: "
            << strerror(errno);
        close(me->sfd_);
        me->sfd_ = -1;
        me->connecting_= false;
        me->NewIdle();
        return;
    }

    me->connecting_ = false;
    me->Connected();
}

void ClientConnection::Connecting()
{
    assert(connected_ == false);
    assert(connecting_ == false);

    connected_ = false;
    connecting_ = true;

    if (event_assign(&event_, channel_->base(), sfd_,
                     EV_WRITE,
                     HandleConnectingEvent, this)) {
        LOG(FATAL) << "set event failed!!!";
    }
    if (event_add(&event_, 0)) {
        LOG(FATAL) << "add event failed!!!";
    }

    /* start the connecting monitor */
    NewWatcher();
}

void ClientConnection::Connect()
{
    int sfd, err = 0;
    sockinfo si;

    assert(connected_ == false);
    assert(connecting_ == false);

    const ChannelOptions &options = channel_->options();
    int port = channel_->port();
    const string& host = channel_->host();

    err = resolve_addr(host.c_str(), port, &si);
    if (err) {
        LOG(ERROR) << "resolve address failed: "
            << host << ":" << port;
        NewIdle();
        return;
    }

    sfd = socket(si.family, SOCK_STREAM, 0);
    if (sfd == -1) {
        LOG(ERROR) << "socket failed: "
            << strerror(errno);
        NewIdle();
        return;
    }

    if (set_nonblocking(sfd)) {
        LOG(ERROR) << "set nonblocking failed: "
            << strerror(errno);
        close(sfd);
        NewIdle();
        return;
    }

    if (set_tcpnodelay(sfd)) {
        LOG(ERROR) << "set tcpnodelay failed: "
            << strerror(errno);
        close(sfd);
        NewIdle();
        return;
    }

    if (set_rcvbuf(sfd, options.rbuf_size)) {
        LOG(ERROR) << "set rcvbuf size failed: "
            << strerror(errno);
        close(sfd);
        NewIdle();
        return;
    }

    if (set_sndbuf(sfd, options.sbuf_size)) {
        LOG(ERROR) << "set sndbuf size failed: "
            << strerror(errno);
        close(sfd);
        NewIdle();
        return;
    }

again:
    err = connect(sfd, (sockaddr *)&si.addr, si.addrlen);
    if (err == 0) {
        sfd_ = sfd;
        Connected();
        return;
    }
    if (errno == EINTR) {
        goto again;
    } else if (errno == EINPROGRESS) {
        sfd_ = sfd;
        Connecting();
    } else {
        DLOG(ERROR) << "connect to "
            << channel_->host()
            << ":"
            << channel_->port()
            << " failed: "
            << strerror(errno);
        close(sfd);
        NewIdle();
    }
}

void ClientConnection::EnableUpload()
{
    if (!connected_) {
        return;
    }
    Connection::EnableUpload();
}

void ClientConnection::DisableUpload()
{
    if (!connected_) {
        return;
    }
    return Connection::DisableUpload();
}

void ClientConnection::SendFail()
{
    channel_->SendFail();
}

void ClientConnection::RecvFail()
{
    channel_->RecvFail();
}

void ClientConnection::SendDone(Message *msg)
{
    channel_->SendDone(msg);
}

bool ClientConnection::SendNext(Message **msg)
{
    return channel_->SendNext(msg);
}

bool ClientConnection::RecvDone(const char *payload, int meta, int data)
{
    return channel_->RecvDone(payload, meta, data);
}

} // namespace qrpc
