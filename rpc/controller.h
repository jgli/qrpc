#ifndef QRPC_RPC_CONTROLLER_H
#define QRPC_RPC_CONTROLLER_H

#include <stdint.h>
#include <event.h>
#include <string>
#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

namespace qrpc {

enum CompressionType {
    kNoCompression      = 0,
    kZlibCompression    = 1,
    kLz4Compression     = 2,
    kSnappyCompression  = 3,
};

/*
 * The min compression message size (byte). 
 * If the message's size is less than this value,
 * the compression behavior will be ignore.
 */
static const int kCompressionThreshold = 256;

struct ControllerOptions {
    /*
     * The rpc call timeout (millisecond).
     *
     * Default: 1000
     */
    int rpc_timeout;

    /*
     * The compression type for rpc message.
     *
     * Default: kNoCompression
     */
    CompressionType compression;

    /* construct function */
    ControllerOptions()
        : rpc_timeout(1000)
        , compression(kNoCompression)
    {
    }
};

class Controller : public google::protobuf::RpcController {
public:
    /**
     * Create a controller with the specified options.
     *
     * Stores a pointer to a heap-allowed channel in *ctlptr
     * and returns zero on success.
     * Stores NULL in *ctlptr and returns an error code on error.
     *
     * Caller should delete *ctlptr when it is no longer needed.
     */
    static int New(const ControllerOptions &options,
                   Controller **ctlptr);

    inline Controller() { }
    virtual ~Controller();

    /**
     * --------------- Client & Server shared methods ---------------
     * These calls can be made from both client side and server side.
     */

    /*
     * Get the local address in format of "ip:port".
     *
     * For client:
     * This method returns the local address where the message send to.
     *
     * For server:
     * This method returns the local address where the message received from.
     */
    virtual std::string LocalAddress() const = 0;

    /*
     * Get the remote address in format of "ip:port".
     *
     * For client:
     * This method returns the remote address where the messsage sent to.
     *
     * For server:
     * This method returns the remote address where the message received from.
     */
    virtual std::string RemoteAddress() const = 0;

    /**
     * -------------------- Client-side methods --------------------
     *
     * These calls may be made from the client side only. Their results
     * are undefined on the server side (may crash).
     */

    /**
     * Resets the RpcController to its initial state so that it may be reused in
     * a new call.  Must not be called while an RPC is in progress.
     */
    virtual void Reset() = 0;

    /**
     * After a call has finished, returns true if the call failed.  The possible 
     * reasons for failure depend on the RPC implementation.  Failed() must not
     * be called before a call has finished.  If Failed() returns true, the
     * contents of the response message are undefined.
     */
    virtual bool Failed() const = 0;

    /**
     * If Failed() is true, returns a human-readable description of the error.
     */
    virtual std::string ErrorText() const = 0;

    /**
     * Advises the RPC system that the caller desires that the RPC call be
     * canceled.  The RPC system may cancel it immediately, may wait awhile and
     * then cancel it, or may not even cancel the call at all.  If the call is
     * canceled, the "done" callback will still be called and the RpcController
     * will indicate that the call failed at that time.
     */
    virtual void StartCancel() = 0;

    /**
     * -------------------- Server-side methods --------------------
     *
     * These calls may be made from the server side only.  Their results
     * are undefined on the client side (may crash).
     */

    /**
     * Causes Failed() to return true on the client side.  "reason" will be
     * incorporated into the message returned by ErrorText().  If you find
     * you need to return machine-readable information about failures, you
     * should incorporate it into your response protocol buffer and should
     * NOT call SetFailed().
     */
    virtual void SetFailed(const std::string &reason) = 0;

    /**
     * If true, indicates that the client canceled the RPC, so the server may
     * as well give up on replying to it.  The server should still call the
     * final "done" callback.
     */
    virtual bool IsCanceled() const = 0;

    /**
     * Asks that the given callback be called when the RPC is canceled.  The
     * callback will always be called exactly once.  If the RPC completes without
     * being canceled, the callback will be called after completion.  If the RPC
     * has already been canceled when NotifyOnCancel() is called, the callback
     * will be called immediately.
     *
     * NotifyOnCancel() must be called no more than once per request.
     */
    virtual void NotifyOnCancel(google::protobuf::Closure *callback) = 0;

private:
    /* No copying allowed */
    Controller(const Controller &);
    void operator=(const Controller &);
};

} // namespace qrpc

#endif /* QRPC_RPC_CONTROLLER_H */
