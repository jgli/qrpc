#include <stdlib.h>
#include <stdio.h>
#include <event.h>
#include <string>

#include "src/qrpc/util/log.h"
#include "src/qrpc/rpc/rpc.h"
#include "src/qrpc/example/echo.pb.h"

using namespace std;
using namespace qrpc;
using namespace test;
using namespace google::protobuf;

void EchoDone(Controller *controller, EchoRequest *request, EchoResponse *response, event_base *base)
{
    if (controller->Failed()) {
        LOG(ERROR) << "RPC " << controller->LocalAddress()
            << " ---> " << controller->RemoteAddress()
            << " failed, reason: " << controller->ErrorText();
    } else {
        LOG(INFO) << "RPC " << controller->LocalAddress()
            << " ---> " << controller->RemoteAddress()
            << " success, result: " << response->result();
    }

    /* JUST run once time */
    event_base_loopbreak(base);
}

int main()
{
    //google::InitGoogleLogging("cli");

    event_base *base = event_base_new();

    Channel *channel = NULL;
    int rc = Channel::New(ChannelOptions(), "127.0.0.1", 11111, base, &channel);
    if (rc) {
        return -1;
    }

    rc = channel->Open();
    if (rc) {
        return -1;
    }

    Controller *controller = NULL;
    rc = Controller::New(ControllerOptions(), &controller);
    if (rc) {
        return -1;
    }

    EchoRequest *request = new EchoRequest();
    request->set_query("client");

    EchoResponse *response = new EchoResponse();

    EchoService::Stub stub(channel);
    stub.Echo(controller, request, response, qrpc::NewCallback(&EchoDone, controller, request, response, base));

    event_base_loop(base, 0);

    delete response;
    delete request;
    delete controller;
    delete channel;
    event_base_free(base);

    google::protobuf::ShutdownProtobufLibrary();
    //google::ShutdownGoogleLogging();

    return 0;
}
