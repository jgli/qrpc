#include <stdlib.h>
#include <stdio.h>
#include <event.h>
#include <stack>
#include <vector>
#include <string>

#include <gflags/gflags.h>
#include <glog/logging.h>

#include "qrpc/util/random.h"
#include "qrpc/util/completion.h"
#include "qrpc/rpc/rpc.h"
#include "echo.pb.h"

using namespace std;
using namespace qrpc;
using namespace test;
using namespace google;
using namespace google::protobuf;
using namespace GFLAGS_NAMESPACE;

DEFINE_string(host, "127.0.0.1", "The ip of the server");
DEFINE_int32(port, 44444, "The port of the server");

DEFINE_int32(msg_size, 1, "The size in bytes of a request");
DEFINE_int32(compress, 0, "The compression type (0: no, 1: zlib, 2: Lz4, 3: snappy)");
DEFINE_int32(rpc_timeout, 50000, "The rpc timeout in millisecond");

DEFINE_uint64(worker_num, 4, "The number of worker threads");
DEFINE_uint64(total_num, 10000, "The total number of requests for each channel");
DEFINE_uint64(per_cons, 1, "The number of channels for each worker thread");
DEFINE_uint64(per_reqs, 1, "The number of request for each channel to sending");

class Worker;

string rpc_msg;
timeval stop_time;
timeval start_time;
vector<Worker *> workers;
vector<pthread_t> threads;

struct Msg {
    timeval start_time_;
    timeval stop_time_;

    EchoRequest request_;
    EchoResponse response_;

    qrpc::Controller *controller_;

    Msg() : controller_(NULL) { }
    ~Msg() { delete controller_; }
};

class Connection {
public:
    Worker *worker_;
    event_base *base_;

    stack<Msg *> temps_;
    vector<Msg *> results_;

    Channel *channel_;
    EchoService::Stub *stub_;

public:
    Connection(Worker *worker, event_base *base)
        : worker_(worker)
        , base_(base)
        , channel_(NULL)
        , stub_(NULL)
    {
        for (uint64_t i = 0; i < FLAGS_total_num; ++i) {
            Msg *item = new Msg();

            item->request_.set_query(rpc_msg);

            ControllerOptions options;
            options.rpc_timeout = FLAGS_rpc_timeout;
            options.compression = (CompressionType)FLAGS_compress;
            
            int rc = Controller::New(options, &item->controller_);
            if (rc) {
                LOG(FATAL) << "alloc controller failed";
            }

            temps_.push(item);
        }
    }

    ~Connection()
    {
        for (size_t i = 0; i < results_.size(); ++i) {
            delete results_[i];
        }

        delete channel_;
        delete stub_;
    }

    void StartPerf()
    {
        int rc = Channel::New(ChannelOptions(),
                FLAGS_host, FLAGS_port, base_, &channel_);
        if (rc) {
            LOG(FATAL) << "alloc channel failed";
        }

        rc = channel_->Open();
        if (rc) {
            LOG(FATAL) << "open channel failed";
        }

        stub_ = new EchoService::Stub(channel_);
        if (!stub_) {
            LOG(FATAL) << "alloc broker service stub failed";
        }

        for (uint64_t i = 0; i < FLAGS_per_reqs; ++i) {
            Run();
        }
    }

    void Done(Msg *item);

    void Run();
};

class Worker {
public:
    uint64_t fin_;
    Completion work_;
    event_base *base_;
    vector<Connection *> conns_;

public:
    Worker() : fin_(0) , work_(1), base_(NULL)
    {
        base_ = event_base_new();
        if (!base_) {
            LOG(FATAL) << "new event base failed";
        }

        for (uint64_t i = 0; i < FLAGS_per_cons; i++) {
            Connection *conn = new Connection(this, base_);
            if (!conn) {
                LOG(FATAL) << "new connection failed";
            }
            conns_.push_back(conn);
        }
    }

    ~Worker()
    {
        for (size_t i = 0; i < conns_.size(); ++i) {
            delete conns_[i];
        }

        event_base_free(base_);
    }

    void StartPerf()
    {
        work_.Signal();
    }

    void WaitForPerf()
    {
        work_.Wait();

        for (size_t i = 0; i < conns_.size(); ++i) {
            conns_[i]->StartPerf();
        }
    }

    void Finish()
    {
        if (++fin_ < FLAGS_per_cons) {
            return;
        }

        event_base_loopbreak(base_);
    }
};

void Connection::Done(Msg *item)
{
    if (item->controller_->Failed()) {
        LOG(FATAL) << "RPC response error: " << item->controller_->ErrorText();
    } else {
        gettimeofday(&item->stop_time_, NULL);
        results_.push_back(item);
        Run();
    }
}

void Connection::Run()
{
    if (results_.size() == FLAGS_total_num) {
        worker_->Finish();
    }

    if (temps_.empty()) {
        return;
    }

    Msg *item = temps_.top();
    temps_.pop();

    stub_->Echo(item->controller_,
            &item->request_, &item->response_,
            qrpc::NewCallback(this, &Connection::Done, item));
    gettimeofday(&item->start_time_, NULL);
}

void* worker_routine(void *arg)
{
    struct father {
        pthread_t tid;
        Worker *worker;
        Completion *work;
    };

    Worker *me = new Worker();
    if (!me) {
        LOG(FATAL) << "alloc worker failed";
    }

    father *f = (father *)arg;
    f->tid = pthread_self();
    f->worker = me; 
    f->work->Signal();

    me->WaitForPerf();

    event_base_loop(me->base_, 0);

    return NULL;
}

uint64_t escape_us(timeval &start, timeval &stop)
{
    uint64_t end = 0;
    uint64_t fir = 0;

    fir = start.tv_sec * 1000000 + start.tv_usec;
    end = stop.tv_sec * 1000000 + stop.tv_usec;

    return end - fir;
}

uint64_t total_requests_time_us()
{
    uint64_t time = 0;

    for (size_t i = 0; i < workers.size(); ++i) {
        Worker *w = workers[i];

        for (size_t j = 0; j < w->conns_.size(); ++j) {
            Connection *c = w->conns_[j];

            for (size_t k = 0; k < c->results_.size(); ++k) {
                Msg *msg = c->results_[k];

                time += escape_us(msg->start_time_, msg->stop_time_);
            }
        }
    }

    return time;
}

int main(int argc, char *argv[])
{
    /* init argument */
    ParseCommandLineFlags(&argc, &argv, false);

    /* init log prefix */
    google::InitGoogleLogging("cli");

    /* init random message */
    for (int i = 0; i < FLAGS_msg_size; ++i) {
        uint64_t c = random_range('a', 'z');
        rpc_msg += (char)c;
    }

    /* init worker threads */
    for (uint64_t i = 0; i < FLAGS_worker_num; i++) {
        Completion work(1);

        struct {
            pthread_t tid;
            Worker *worker;
            Completion *work;
        } arg = {0, NULL, &work};

        pthread_t tid;
        int rc = pthread_create(&tid, NULL, worker_routine, &arg);
        if (rc) {
            exit(EXIT_FAILURE);
        }
        work.Wait();

        threads.push_back(arg.tid);
        workers.push_back(arg.worker);
    }

    /* start sending requests */
    gettimeofday(&start_time, NULL);
    for (uint64_t i = 0; i < FLAGS_worker_num; i++) {
        workers[i]->StartPerf();
    }

    /* wait finishing requests */
    for (uint64_t i = 0; i < FLAGS_worker_num; i++) {
        pthread_join(threads[i], NULL);
    }
    gettimeofday(&stop_time, NULL);

    /* stat the request */
    uint64_t total_request = FLAGS_worker_num * FLAGS_per_cons * FLAGS_total_num;
    uint64_t all_time = total_requests_time_us();
    uint64_t total_time = escape_us(start_time, stop_time);

    printf("qps                  : %lu\n", total_request * 1000000 / total_time);
    printf("per request time(us) : %lu\n", all_time / total_request);
    printf("total request        : %lu\n", total_request);
    printf("total time(us)       : %lu\n", total_time);
    printf("all request time(us) : %lu\n", all_time);
    printf("total thread         : %lu\n", FLAGS_worker_num);
    printf("total connection     : %lu\n", FLAGS_worker_num * FLAGS_per_cons);

    /* release workers */
    for (uint64_t i = 0; i < FLAGS_worker_num; i++) {
        delete workers[i];
    }

    ShutdownProtobufLibrary();
    ShutdownGoogleLogging();
    ShutDownCommandLineFlags();

    return 0;
}
