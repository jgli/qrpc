#ifndef QRPC_RPC_CONNECTION_H
#define QRPC_RPC_CONNECTION_H

#include <sys/uio.h>
#include <stdint.h>
#include <event.h>
#include <pthread.h>

#include <list>
#include <map>
#include <queue>
#include <string>

#include "src/qrpc/util/timer.h"
#include "src/qrpc/util/atomic.h"

namespace qrpc {

struct MsgHdr;
class Message;
class ClientMessage;
class ServerMessage;
class Channel;
class ChannelImpl;
class Worker;
class Server;
class ServerImpl;
class Connection;
class Compressor;

class Connection {
protected:
    Connection();
    virtual ~Connection();

    void EnableUpload();
    void DisableUpload();

    virtual void SendFail() = 0;
    virtual void RecvFail() = 0;
    virtual void SendDone(Message *msg) = 0;
    virtual bool SendNext(Message **msg) = 0; 
    virtual bool RecvDone(const char *payload, int meta, int data) = 0;

protected:
    enum State {
        kListen         = 0,
        kWait           = 1,
        kRead           = 2,
        kParse          = 3,
        kWrite          = 4,
        kClose          = 5,
    };

    enum Status {
        kReserved       = 00,

        kRecvError      = 10,
        kRecvOk         = 11,
        kRecvAgain      = 12,

        kSendError      = 20,
        kSendOk         = 21,
        kSendAgain      = 22,
        kSendNothing    = 23,

        kEncodeOk       = 30,
        kEncodeError    = 31,

        kDecodeError    = 40,
        kDecodeOk       = 41,
        kDecodeFragment = 42,
    };

protected:
    bool ExpandWbuf(size_t required);
    bool ExpandRbuf(size_t required);

    Status Encode();
    Status Decode();

    bool OnRecv();
    Status Recv();

    bool OnSend();
    Status Send();

    static void HandleConnectedEvent(int, short, void *);

protected:
    int             sfd_;
    State           rstate_;
    State           wstate_;
    event           event_;

    int             rsize_;
    int             rbytes_;
    char*           rcur_;
    char*           rbuf_;
    char*           rmsg_;
    MsgHdr          rmsg_hdr_;

    Message*        wmsg_;
    int             wsize_;
    int             wbytes_;
    char*           wcur_;
    char*           wbuf_;
    char*           msg_body_;
    char*           hdr_comp_;
    char*           hdr_meta_;
    char*           hdr_data_;
    char*           hdr_payload_;

    Compressor*     compressor_;
    
private:
    /* No copying allowed */
    Connection(const Connection &);
    void operator=(const Connection &);
};

class ServerConnection : public Connection {
public:
    explicit ServerConnection(Worker *worker, int sfd,
            std::string &local_addr, std::string &remote_addr);
    virtual ~ServerConnection();

    void Close();
    void Send(ServerMessage *msg);

    Worker* worker() const     { return worker_;      }
    std::string& local_addr()  { return local_addr_;  }
    std::string& remote_addr() { return remote_addr_; }

private:
    void CloseConnection();
    void ReleaseConnection();

    uint64_t CurClock();
    void NewClockKeepalive();
    void DelClockKeepalive();
    void HandleClockKeepalive();

    void NewTimerKeepalive();
    void UpdTimerKeepalive();
    void DelTimerKeepalive();
    void HandleTimerKeepalive();

    void OnRpcCancel(const MsgMeta &meta);
    void OnRpcFinish(ServerMessage *msg);
    void OnRpcRequest(ServerMessage *msg);
    void OnRpcResponse(ServerMessage *msg);

    virtual void SendFail();
    virtual void RecvFail();
    virtual void SendDone(Message *msg);
    virtual bool SendNext(Message **msg); 
    virtual bool RecvDone(const char *payload, int meta, int data);

private:
    typedef std::pair<uint64_t, ServerMessage *> MsgItem;
    typedef std::list<std::pair<uint64_t, ServerMessage *> > MsgQueue;

    MsgQueue::iterator find_if(MsgQueue &msgq, uint64_t seq);

private:
    Worker *worker_;

    MsgQueue recvq_;
    MsgQueue sendq_;
    MsgItem cur_send_;

    Timer timer_;
    uint64_t update_;
    bool has_timer_; 
    bool use_clock_;
    bool connected_;

    std::string local_addr_;
    std::string remote_addr_;
};

class ClientConnection : public Connection {
public:
    explicit ClientConnection(ChannelImpl *channel);
    virtual ~ClientConnection();

    void EnableUpload();
    void DisableUpload();

    ChannelImpl* channel_impl() { return channel_;     }
    std::string& local_addr()   { return local_addr_;  }
    std::string& remote_addr()  { return remote_addr_; }

private:
    void DelTimer();

    void NewIdle();
    void DelIdle();

    void NewWatcher();
    void DelWatcher();

    void NewHeartbeat();
    void DelHeartbeat();

    void Connected();
    void Connect();
    void Connecting();

    void HandleIdleEvent();
    void HandleWatchEvent();
    void HandleHeartbeatEvent();
    static void HandleConnectingEvent(int, short, void *);

    virtual void SendFail();
    virtual void RecvFail();
    virtual void SendDone(Message *msg);
    virtual bool SendNext(Message **msg); 
    virtual bool RecvDone(const char *payload, int meta, int data);

private:
    ChannelImpl *channel_;
    bool connected_;
    bool connecting_;
    bool has_timer_; 
    Timer timer_;

    std::string local_addr_;
    std::string remote_addr_;
};

} // namespace qrpc

#endif /* QRPC_RPC_CONNECTION_H */
