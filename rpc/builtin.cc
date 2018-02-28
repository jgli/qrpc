#include <stdlib.h>
#include <assert.h>
#include <string>

#include "src/qrpc/util/log.h"
#include "src/qrpc/rpc/builtin.h"

using namespace std;
using namespace google::protobuf;

namespace qrpc {

BuiltinServiceImpl::BuiltinServiceImpl()
{

}

BuiltinServiceImpl::~BuiltinServiceImpl()
{

}

void BuiltinServiceImpl::Status(google::protobuf::RpcController *controller,
                                const StatusRequest *request,
                                StatusResponse *response,
                                google::protobuf::Closure *done)
{
    done->Run();
}

} // namespace qrpc
