#include <sys/types.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <assert.h>
#include <event.h>
#include <pthread.h>
#include <string>

#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

#include "src/qrpc/util/log.h"
#include "src/qrpc/util/atomic.h"
#include "src/qrpc/util/random.h"
#include "src/qrpc/util/completion.h"
#include "src/qrpc/util/socket.h"
#include "src/qrpc/rpc/errno.h"
#include "src/qrpc/rpc/controller.h"
#include "src/qrpc/rpc/controller_client.h"

using namespace std;
using namespace google::protobuf;

namespace qrpc {

namespace {

#define ZERO_RET(param)                 \
do {                                    \
    if ((param) > 0)                    \
        break;                          \
    LOG(ERROR) << "invalid: " << #param;\
    return false;                       \
} while (0)
#define NEGATIVE_RET(param)             \
do {                                    \
    if ((param) >= 0)                   \
        break;                          \
    LOG(ERROR) << "invalid: " << #param;\
    return false;                       \
} while (0)
#define NULL_RET(param)                 \
do {                                    \
    if (!param.empty())                 \
        break;                          \
    LOG(ERROR) << "invalid: " << #param;\
    return false;                       \
} while (0)

inline bool options_ok(const ControllerOptions &opt)
{
    ZERO_RET(opt.rpc_timeout);

    if (opt.compression > kSnappyCompression) {
        LOG(ERROR) << "invalid compression type";
        return false;
    }

    return true;
}

#undef ZERO_RET
#undef NULL_RET
#undef NEGATIVE_RET

} // anonymous namespace


Controller::~Controller()
{

}

int Controller::New(const ControllerOptions &options, Controller **ctlptr)
{
    if (!options_ok(options)) {
        return kErrParam;
    }

    ClientController *controller = new ClientController(options);
    if (!controller) {
        LOG(ERROR) << "alloc controller object failed!!!";
        return kErrMem;
    }

    *ctlptr = controller;
    return kOk;
}

} // namespace qrpc
