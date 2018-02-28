#ifndef QRPC_RPC_SERVER_IMPL_H
#define QRPC_RPC_SERVER_IMPL_H

#include <stdint.h>
#include <event.h>
#include <pthread.h>

#include <map>
#include <string>

#include "src/qrpc/util/timer.h"
#include "src/qrpc/util/atomic.h"
#include "src/qrpc/util/socket.h"
#include "src/qrpc/util/compiler.h"
#include "src/qrpc/rpc/message.pb.h"

namespace qrpc {

/* defined in other files */
class Worker;
class Server;
class Listener;
class BuiltinServiceImpl;

class ServerImpl : public Server {
public:
    explicit ServerImpl(const ServerOptions &options,
                        event_base *base);
    virtual ~ServerImpl();

    virtual int Add(const std::string &host, int port);
    virtual int Start();
    virtual int Stop();

    virtual int Register(google::protobuf::Service *service,
                         ServiceOwnership ownership);
    virtual int Unregister(std::string service_full_name);
    virtual int Unregister(google::protobuf::Service *service);

public:
    google::protobuf::Service* Find(const MsgMeta &meta) const {
        using namespace google::protobuf;
        using namespace std;

        Service *service = NULL;

        map<string, Service *>::const_iterator it;
        it = services_.find(meta.service());
        if (it != services_.end()) {
            service = it->second;
        }

        return service;
    }

    const ServerOptions& options() { return options_; }
    event_base* base() { return base_; }

    bool Dispatch(int sfd, std::string &local, std::string &remote);

private:
    const std::string& state() const;

    void DelService();

    bool NewWorker();
    void DelWorker();

    void DelServer();
    bool NewServer();
    void StopServer();
    bool StartServer();

private:
    /* call Start/Stop Server */
    friend class Worker;

    enum State {
        kInit = 0,
        kRun  = 1,
        kExit = 2
    };

    ServerOptions options_;
    State state_;

    /* worker's event base if (has_base_ == false) */
    bool has_base_;
    event_base *base_;

    /* listen sockets */
    std::vector<Listener *> listens_;
    std::vector<std::pair<std::string, int> > endpoints_;

    /* worker threads */
    int nxt_worker_;
    std::vector<Worker *> workers_;
    std::map<pthread_t, Worker *> each_workers_;

    /* shared builtin service */
    BuiltinServiceImpl builtin_service_;

    pthread_t tid_;
    //pthread_rwlock_t service_lock_;
    std::map<std::string, ServiceOwnership> ownership_;
    std::map<std::string, google::protobuf::Service *> services_;
};

} // namespace qrpc

#endif /* QRPC_RPC_SERVER_IMPL_H */
