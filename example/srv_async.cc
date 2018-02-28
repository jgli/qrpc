#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <event.h>
#include <pthread.h>
#include <time.h>
#include <list>
#include <string>
#include <tr1/functional>

#include "src/qrpc/util/log.h"
#include "src/qrpc/rpc/rpc.h"
#include "src/qrpc/example/echo.pb.h"

using namespace std;
using namespace qrpc;
using namespace test;
using namespace google::protobuf;

struct Produce {
    const test::EchoRequest *request_;
    test::EchoResponse *response_;
    google::protobuf::Closure *done_;
    google::protobuf::RpcController *controller_;
};

class WorkerThread {
public:
    WorkerThread(event_base *base) : base_(base) { StartTimer(); }
    ~WorkerThread() { StopTimer(); }

    void Recv(Produce *item) { items_.push_back(item); }

private:
    void StartTimer() {
        timeval tv = {1, 0};
        evtimer_set(&event_, HandleTimeout, this);
        event_base_set(base_, &event_);
        evtimer_add(&event_, &tv);
    }

    void StopTimer() {
        evtimer_del(&event_);
    }

    void OnTimeout() {
        for (list<Produce *>::iterator it = items_.begin();
             it != items_.end(); ++it) {
            (*it)->response_->set_result("server");
            (*it)->done_->Run();

            LOG(INFO) << "RPC response thread: " << pthread_self();

            delete (*it);
        }

        items_.clear();

        StartTimer();
    }

    static void HandleTimeout(int fd, short what, void *data) {
        WorkerThread *me = (WorkerThread *)data;
        me->OnTimeout();
    }

private:
    event_base *base_;
    event event_;
    std::list<Produce *> items_;
};

class EchoServiceImpl : public EchoService {
public:
    EchoServiceImpl() { }
    virtual ~EchoServiceImpl() { }
    
    virtual void Echo(::google::protobuf::RpcController* controller,
                      const ::test::EchoRequest* request,
                      ::test::EchoResponse* response,
                      ::google::protobuf::Closure* done) {
        Controller *qrpc_ctl = (Controller *)controller;

        LOG(INFO) << "RPC " << qrpc_ctl->RemoteAddress()
            << " ---> " << qrpc_ctl->LocalAddress()
            << " request: " << request->query()
            << ", thread: " << pthread_self();

        Produce *item = new Produce();
        item->request_ = request;
        item->response_ = response;
        item->done_ = done;
        item->controller_ = controller;

        worker_->Recv(item);
    }

    void InitWorker(qrpc::Thread *thr) {
        //EchoServiceImpl *me = (EchoServiceImpl *)data;

        worker_ = new WorkerThread(thr->base());
    }

    void ExitWorker(qrpc::Thread *thr) {
        delete worker_;
    }

private:
    static __thread WorkerThread *worker_;
};

__thread WorkerThread *EchoServiceImpl::worker_ = NULL;

void OnSigint(int sig, short why, void *data)
{
    event_base *base = (event_base *)data;

    event_base_loopexit(base, NULL);
}

int main()
{
    //google::InitGoogleLogging("srv");

    event_base *base = event_base_new();

    event *sigint = evsignal_new(base, SIGINT, OnSigint, base);
    if (!sigint) {
        return -1;
    }
    if (evsignal_add(sigint, NULL)) {
        return -1;
    }

    EchoServiceImpl *service = new EchoServiceImpl();

    ServerOptions options;
    options.init_cb  = tr1::bind(&EchoServiceImpl::InitWorker, service, tr1::placeholders::_1);
    options.exit_cb  = tr1::bind(&EchoServiceImpl::ExitWorker, service, tr1::placeholders::_1);

    Server *server = NULL;
    int rc = Server::New(options, base, &server);
    if (rc) {
        return -1;
    }

    rc = server->Register(service, kServerOwnsService);
    if (rc) {
        return -1;
    }

    rc = server->Add("127.0.0.1", 11111);
    if (rc) {
        return -1;
    }

    rc = server->Start();
    if (rc) {
        return -1;
    }

    event_base_loop(base, 0);

    delete server;
    event_free(sigint);
    event_base_free(base);

    google::protobuf::ShutdownProtobufLibrary();
    //google::ShutdownGoogleLogging();

    return 0;
}
