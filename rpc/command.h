#ifndef QRPC_RPC_COMMAND_H
#define QRPC_RPC_COMMAND_H

#include <stdint.h>
#include <string>

#include "src/qrpc/util/atomic.h"
#include "src/qrpc/util/completion.h"
#include "src/qrpc/util/task.h"

namespace qrpc {

class Worker;
class ServerImpl;

/* accept a new socket */
class Link : public Task {
public:
    explicit Link(Worker *worker, int sfd,
            std::string &local, std::string &remote);
    virtual ~Link();

    virtual void Quit();
    virtual void operator()();

public:
    /* accepted socket fd */
    int sfd_;
    Worker *worker_;
    std::string local_;
    std::string remote_;

private:
    /* No copying allowed */
    Link(const Link &);
    void operator=(const Link &);
};

/* listen socket */
class Listen : public Task {
public:
    explicit Listen(ServerImpl *impl,
            Worker *worker,
            bool listen, Completion &work);
    virtual ~Listen();

    virtual void Quit();
    virtual void operator()();

public:
    bool res_;
    bool listen_;
    Worker *worker_;
    ServerImpl *impl_;
    Completion &work_;

private:
    /* No copying allowed */
    Listen(const Listen &);
    void operator=(const Listen &);
};

} // namespace qrpc

#endif /* QRPC_RPC_COMMAND_H */
