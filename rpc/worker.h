#ifndef QRPC_RPC_WORKER_H
#define QRPC_RPC_WORKER_H

#include <stdint.h>
#include <pthread.h>
#include <event.h>
#include <map>
#include <list>
#include <string>

#include "src/qrpc/util/timer.h"
#include "src/qrpc/util/thread.h"
#include "src/qrpc/util/event_queue.h"

namespace qrpc {

class Compressor;
class Quit;
class Link;
class Listen;
class Server;
class ServerImpl;
class Message;
class ServerMessage;
class Connection;
class ServerConnection;

class Worker {
public:
    explicit Worker(ServerImpl *server);
    ~Worker();

    /* handle link socket */
    void Link(::qrpc::Link *cmd);
    void HandleLink(::qrpc::Link *cmd);

    /* handle listen socket */
    void Listen(::qrpc::Listen *cmd);
    void HandleListen(::qrpc::Listen *cmd);

    void Unlink(ServerConnection *conn);

public:
    ServerImpl* server_impl() { return server_;                }
    Compressor* compressor()  { return compressor_;            }
    event_base* base()        { return bg_thread_->base();     }
    Thread*     thread()      { return bg_thread_;             }
    EvQueue*    ev_queue()    { return bg_thread_->ev_queue(); }

private:
    /* init thread local variable */
    void InitWorker(Thread *thr);

    /* exit thread local variable */
    void ExitWorker(Thread *thr);

private:
    typedef std::pair<void *, ServerConnection *> Client;
    typedef std::map<void *, ServerConnection *> ClientQueue;

private:
    ServerImpl *server_;

    /* peer connections */
    ClientQueue clients_;

    /* thread based compressor */
    Compressor *compressor_;

    /* event queue based thread */
    Thread *bg_thread_;

private:
    /* No copying allowed */
    Worker(const Worker &);
    void operator=(const Worker &);
};

} // namespace qrpc

#endif /* QRPC_RPC_WORKER_H */
