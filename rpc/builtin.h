#ifndef QRPC_RPC_BUILTIN_H
#define QRPC_RPC_BUILTIN_H

#include <stdint.h>
#include <pthread.h>
#include <string>
#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include "src/qrpc/rpc/builtin.pb.h"

namespace qrpc {

class BuiltinServiceImpl : public BuiltinService {
public:
    BuiltinServiceImpl();
    virtual ~BuiltinServiceImpl();

    virtual void Status(::google::protobuf::RpcController* controller,
                        const StatusRequest* request,
                        StatusResponse* response,
                        ::google::protobuf::Closure* done);
};

} // namespace qrpc

#endif /* QRPC_RPC_BUILTIN_H */
