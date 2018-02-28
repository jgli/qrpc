#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <event.h>
#include <string>

#include <gflags/gflags.h>
#include <glog/logging.h>

//#include "src/util/flags.h"
#include "src/qrpc/rpc/rpc.h"
#include "src/qrpc/benchmark/echo.pb.h"

using namespace std;
using namespace qrpc;
using namespace test;
using namespace google;
using namespace google::protobuf;

DEFINE_string(host, "127.0.0.1", "The ip of the server");
DEFINE_int32(port, 44444, "The port of the server");
DEFINE_int32(thread, 4, "The number of worker threads");

class EchoServiceImpl : public EchoService {
public:
    EchoServiceImpl() { }
    virtual ~EchoServiceImpl() { }
    
    virtual void Echo(::google::protobuf::RpcController* controller,
                      const ::test::EchoRequest* request,
                      ::test::EchoResponse* response,
                      ::google::protobuf::Closure* done) {
        response->set_result("ok");
        //response->set_result(request->query());

        done->Run();
    }
};

void OnSigint(int sig, short why, void *data)
{
    event_base *base = (event_base *)data;

    event_base_loopexit(base, NULL);
}

int main(int argc, char *argv[])
{
    /* init argument */
    ParseCommandLineFlags(&argc, &argv, false);

    /* init log prefix */
    InitGoogleLogging("srv");

    //google::InitGoogleLogging("srv");

    event_base *base = event_base_new();

    event *sigint = evsignal_new(base, SIGINT, OnSigint, base);
    if (!sigint) {
        return -1;
    }
    if (evsignal_add(sigint, NULL)) {
        return -1;
    }

    ServerOptions options;
    options.num_worker_thread = FLAGS_thread;

    Server *server = NULL;
    int rc = Server::New(options, base, &server);
    if (rc) {
        return -1;
    }

    rc = server->Register(new EchoServiceImpl(), kServerOwnsService);
    if (rc) {
        return -1;
    }

    rc = server->Add(FLAGS_host, FLAGS_port);
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

    ShutdownProtobufLibrary();
    ShutdownGoogleLogging();
    ShutDownCommandLineFlags();

    return 0;
}
