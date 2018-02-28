#ifndef QRPC_RPC_CHANNEL_H
#define QRPC_RPC_CHANNEL_H

#include <stdint.h>
#include <event.h>
#include <string>
#include <google/protobuf/service.h>
#include <google/protobuf/descriptor.h>

namespace qrpc {

struct ChannelOptions {
    /*
     * The recv buf size (bytes) in kernel mode.
     *
     * Default: 16KB
     */
    int rbuf_size;
    
    /*
     * The send buf size (bytes) in kernel mode.
     *
     * Default: 16KB
     */
    int sbuf_size;

    /*
     * The low watermark of recving buf size (bytes)
     * in user mode.
     *
     * Default: 32KB
     */
    int min_rbuf_size;

    /*
     * The high watermark of recv buf size (bytes)
     * in user mode.
     *
     * Default: 1MB
     */
    int max_rbuf_size;

    /*
     * The low watermark of sending buf size (bytes)
     * in user mode.
     *
     * Default: 32KB
     */
    int min_sbuf_size;

    /*
     * The high watermark of sending buf size (bytes)
     * in user mode.
     *
     * Default: 1MB 
     */
    int max_sbuf_size;

    /*
     * The connect timeout (millisecond).
     * It will retry connecting if the previous connection failed.
     *
     * Default: 5000
     */
    int connect_timeout;

    /*
     * The retry interval (millisecond) to retry connecting.
     *
     * Default: 1000
     */
    int retry_interval;

    /*
     * The heartbeat interval (millisecond).
     * ZERO means disable this feature.
     *
     * Default: 600000 (10 min)
     */
    int heartbeat_interval;

    /* construct function */
    ChannelOptions();
};

class Channel : public google::protobuf::RpcChannel {
public:
    /**
     * Create a channel with the specified options.
     *
     * The host specifies either a numerical network address (
     * for IPv4, numbers-and-dots notation;
     * for IPv6, hexadecimal string format),
     * or a network hostname, whose network addresses are looked up
     * and the first address resolved will be used.
     *
     * Stores a pointer to a heap-allowed channel in *chanptr
     * and returns zero on success.
     * Stores NULL in *chanptr and returns an error code on error.
     *
     * Caller should delete *chanptr when it is no longer needed.
     */
    static int New(const ChannelOptions &options,
                   const std::string &host, int port,
                   event_base *base,
                   Channel **chanptr);

    inline Channel() { }
    virtual ~Channel();

    /**
     * Open the channel and connect to the remote server. 
     *
     * It will retry connecting if failed.
     *
     * @return
     * Return 0 if success, error code otherwise.
     */
    virtual int Open() = 0;

    /**
     * Close the channel and close the connection with remote server.
     *
     * It will cancel all pending requests.
     *
     * @return
     * Return 0 if success, error code otherwise.
     */
    virtual int Close() = 0;

    /**
     * Cancel all pending requests, but don't close the channel.
     *
     * It wraps all pending requests's rpc::Controller::StartCancel().
     *
     * @return
     * Return 0 if success, error code otherwise.
     */
    virtual int Cancel() = 0;

private:
    /* No copying allowed */
    Channel(const Channel &);
    void operator=(const Channel &);
};

} // namespace qrpc

#endif /* QRPC_RPC_CHANNEL_H */
