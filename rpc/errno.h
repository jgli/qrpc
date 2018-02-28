#ifndef QRPC_RPC_ERRNO_H
#define QRPC_RPC_ERRNO_H

#include <string>

namespace qrpc {

enum Code {
    kOk         = 0,    /* success                          */
    kError      = 1,    /* unknown error                    */
    kErrParam   = 2,    /* invalid argument                 */
    kErrMem     = 3,    /* out of memory                    */
    kErrCtx     = 4,    /* running in wrong thread context  */
    kErrHasSrv  = 5,    /* the service is registered        */
    kErrNotSrv  = 6,    /* the service isn't registered     */
    kErrField   = 7,    /* protobuf required member error   */
    kErrCancel  = 8,    /* the RPC is canceled              */
    kErrTimeout = 9,    /* the RPC is timeout               */
    kErrResponse= 10,   /* the RPC's response message error */
    kErrUserDef = 11,   /* identify app's error text        */
};

extern const std::string& rerror(int rc);

} // namespace qrpc

#endif /* QRPC_RPC_ERRNO_H */
