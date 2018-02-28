#include <string>

#include "src/qrpc/util/log.h"
#include "src/qrpc/rpc/errno.h"

using namespace std;

namespace qrpc {

const string& rerror(int rc)
{
    static string err_msg[] = {
        /* kOk          */  "ok",
        /* kError       */  "unknown error",
        /* kErrParam    */  "invalid argument",
        /* kErrMem      */  "out of memory",
        /* kErrCtx      */  "running in wrong thread context",
        /* kErrHasSrv   */  "the service is registered",
        /* kErrNotSrv   */  "the service isn't registered",
        /* kErrField    */  "protobuf required member error",
        /* kErrCancel   */  "the RPC is canceled",
        /* kErrTimeout  */  "the RPC is timeout",
        /* kErrResponse */  "the RPC's response message error",
        /* kErrUserDef  */  "identify app's error text",
    };

    static string what_is_the_fuck = "Are you fucking kidding me";

    switch (rc) {
    case kOk:
    case kError:
    case kErrParam:
    case kErrMem:
    case kErrCtx:
    case kErrHasSrv:
    case kErrNotSrv:
    case kErrField:
    case kErrCancel:
    case kErrTimeout:
    case kErrResponse:
        return err_msg[rc];
    case kErrUserDef:
        LOG(FATAL) << "shouldn't run here";
    default:
        return what_is_the_fuck;
    }
}

} // namespace qrpc
