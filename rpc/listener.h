#ifndef QRPC_RPC_LISTENER_H
#define QRPC_RPC_LISTENER_H

#include <stdint.h>
#include <event.h>
#include <string>

namespace qrpc {

struct sockinfo;
class ServerImpl;

class Listener {
public:
    explicit Listener(ServerImpl *server_impl);
    ~Listener();

    bool Start(sockinfo &si);

    int          fd()          { return fd_;          }
    std::string  endpoint()    { return endpoint_;    }
    ServerImpl*  server_impl() { return server_impl_; }

private:
    bool UnresolveAddress();
    bool BuildEvent();
    bool BuildSocket(sockinfo &si);
    static void HandleAccept(int fd, short what, void *data);

private:
    int fd_;
    event event_;
    std::string endpoint_;

    ServerImpl *server_impl_;

private:
    /* No copying allowed */
    Listener(const Listener &);
    void operator=(const Listener &);
};

} // namespace qrpc

#endif /* QRPC_RPC_LISTENER_H */
