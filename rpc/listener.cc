#include <sys/types.h>
#include <sys/syscall.h>
#include <sys/ioctl.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <string.h>
#include <string>
#include <algorithm>

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include "src/qrpc/util/log.h"
#include "src/qrpc/util/atomic.h"
#include "src/qrpc/util/random.h"
#include "src/qrpc/util/completion.h"
#include "src/qrpc/util/socket.h"
#include "src/qrpc/rpc/errno.h"
#include "src/qrpc/rpc/worker.h"
#include "src/qrpc/rpc/command.h"
#include "src/qrpc/rpc/message.pb.h"
#include "src/qrpc/rpc/builtin.h"
#include "src/qrpc/rpc/server.h"
#include "src/qrpc/rpc/server_impl.h"
#include "src/qrpc/rpc/listener.h"

using namespace std;
using namespace google::protobuf;

namespace qrpc {

Listener::Listener(ServerImpl *server_impl)
    : fd_(-1)
    , server_impl_(server_impl)
{

}

Listener::~Listener()
{
    if (event_initialized(&event_)) {
        event_del(&event_);
    }
    if (fd_ != -1) {
        close(fd_);
    }
}

bool Listener::Start(sockinfo &si)
{
    if (fd_ != -1) {
        LOG(ERROR) << "the listener maybe running";
        return false;
    }

    if (!BuildSocket(si)) {
        LOG(ERROR) << "create socket failed";
        return false;
    }

    if (!BuildEvent()) {
        LOG(ERROR) << "too many events added";
        return false;
    }

    if (!UnresolveAddress()) {
        LOG(ERROR) << "unresolve socket address failed";
        return false;
    }

    return true;
}

bool Listener::BuildSocket(sockinfo &si)
{
    int err = 0, sfd;

    sfd = socket(si.family, SOCK_STREAM, 0);
    if (sfd == -1) {
        LOG(ERROR) << "open socket failed!!!";
        return false;
    }
    
    if (set_nonblocking(sfd)) {
        LOG(ERROR) << "set nonblocking failed!!!";
        close(sfd);
        return false;
    }
    
    if (set_reuseaddr(sfd)) {
        LOG(ERROR) << "set reuse address failed!!!";
        close(sfd);
        return false;
    }
    
    //if (set_keepalive(sfd)) {
    //    LOG(ERROR) << "set keepalive failed!!!";
    //    close(sfd);
    //    return false;
    //}
    
    if (set_linger(sfd, 0, 0)) {
        LOG(ERROR) << "set linger failed!!!";
        close(sfd);
        return false;
    }
    
    if (set_tcpnodelay(sfd)) {
        LOG(ERROR) << "set tcpnodelay failed!!!";
        close(sfd);
        return false;
    }
    
    err = bind(sfd, (sockaddr *)&si.addr, si.addrlen);
    if (err == -1) {
        LOG(ERROR) << "bind network address failed!!!";
        close(sfd);
        return false;
    }
    
    err = listen(sfd, 1024);
    if (err == -1) {
        LOG(ERROR) << "set listen backlog failed!!!";
        close(sfd);
        return false;
    }

    fd_ = sfd;
    return true;
}

bool Listener::BuildEvent()
{
    assert(fd_ != -1);

    if (event_assign(&event_, server_impl_->base(), fd_,
                     EV_READ | EV_PERSIST,
                     HandleAccept, this)) {
        LOG(ERROR) << "assign event object failed!!!";
        return false;
    }

    if (event_add(&event_, 0)) {
        LOG(ERROR) << "add listen event failed!!!";
        return false;
    }

    return true;
}

bool Listener::UnresolveAddress()
{
    assert(fd_ != -1);

    endpoint_ = unresolve_desc(fd_);

    return true;
}

void Listener::HandleAccept(int fd, short what, void *data)
{
    Listener *me = (Listener *)data;
    int sfd;
    sockaddr addr;
    socklen_t len = sizeof(addr);

    for (; ;) {
        sfd = accept(fd, &addr, &len);

        if (likely(fd > 0)) {
            break;
        }

        if (errno == EINTR) {
            DLOG(INFO) << "accept not ready: interrupt";
            continue;
        } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
            DLOG(INFO) << "accept not ready: error again";
        } else {
            LOG(ERROR) << "accept failed: " << strerror(errno);
        }

        return;
    }

    string peer = unresolve_addr(&addr, len);
    
    const ServerOptions& opt = me->server_impl_->options();

    if (set_rcvbuf(sfd, opt.rbuf_size)) {
        close(sfd);
        LOG(ERROR) << "set rcvbuf size failed: " << strerror(errno);
        return;
    }

    if (set_sndbuf(sfd, opt.sbuf_size)) {
        close(sfd);
        LOG(ERROR) << "set sndbuf size failed: " << strerror(errno);
        return;
    }

    if (set_nonblocking(sfd)) {
        close(sfd);
        LOG(ERROR) << "set nonblocking failed: " << strerror(errno);
        return;
    }

    if (set_tcpnodelay(sfd)) {
        close(sfd);
        LOG(ERROR) << "set tcpnodelay failed: " << strerror(errno);
        return;
    }

    if (!me->server_impl_->Dispatch(sfd, me->endpoint_, peer)) {
        LOG(ERROR) << "dispatch new connected socket failed";
        close(sfd);
    }
}

} // namespace qrpc
