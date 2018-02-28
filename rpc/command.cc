#include <unistd.h>
#include <assert.h>
#include <string>

#include "src/qrpc/util/log.h"
#include "src/qrpc/util/compiler.h"
#include "src/qrpc/util/atomic.h"
#include "src/qrpc/util/completion.h"
#include "src/qrpc/rpc/errno.h"
#include "src/qrpc/rpc/worker.h"
#include "src/qrpc/rpc/command.h"

using namespace std;

namespace qrpc {

Link::Link(Worker *worker, int sfd,
        std::string &local, std::string &remote)
    : sfd_(sfd)
    , worker_(worker)
    , local_(local)
    , remote_(remote)
{

}

Link::~Link()
{

}

void Link::Quit()
{
    close(sfd_);
    delete this;
}

void Link::operator()()
{
    worker_->HandleLink(this);
}

Listen::Listen(ServerImpl *impl, Worker *worker,
               bool listen, Completion &work)
    : res_(true)
    , listen_(listen)
    , worker_(worker)
    , impl_(impl)
    , work_(work)
{

}

Listen::~Listen()
{

}

void Listen::Quit()
{

}

void Listen::operator()()
{
    worker_->HandleListen(this);
}

} // namespace qrpc
