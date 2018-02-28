#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <event.h>
#include <string>

#include "src/qrpc/util/log.h"
#include "src/qrpc/rpc/rpc.h"
#include "src/qrpc/example/echo.pb.h"

using namespace std;
using namespace qrpc;
using namespace test;
using namespace google::protobuf;

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
            << " request: " << request->query();

        response->set_result("server");

        done->Run();
    }
};

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

    Server *server = NULL;
    int rc = Server::New(ServerOptions(), base, &server);
    if (rc) {
        return -1;
    }

    rc = server->Register(new EchoServiceImpl(), kServerOwnsService);
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
