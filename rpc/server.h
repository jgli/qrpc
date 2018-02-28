#ifndef QRPC_RPC_SERVER_H
#define QRPC_RPC_SERVER_H

#include <stdint.h>
#include <event.h>
#include <string>
#include <tr1/functional>
#include <google/protobuf/service.h>

#include "src/qrpc/util/thread.h"

namespace qrpc {

struct ServerOptions {
    /* The callback function of init worker thread */
    typedef std::tr1::function<void(Thread *thr)> Init;
    
    /* The callback function of exit worker thread */
    typedef std::tr1::function<void(Thread *thr)> Exit;

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
     * The low watermark of recv buf size (bytes)
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
     * The keep alive timeout (seconds) for idle sockets.
     * Close the socket if there is no incoming/outgoing request.
     *
     * Default: 3600 (1 hour)
     */
    int keep_alive_time;

    /*
     * The number threads of handing incoming/outgoing requests.
     *
     * Default: 8
     */
    int num_worker_thread;

    /*
     * The init callback function for work thread.
     *
     * Default: NULL
     */
    Init init_cb;

    /*
     * The exit callback function for work thread.
     *
     * Default: NULL
     */
    Exit exit_cb;

    /* construct function */
    ServerOptions();
};

/*
 * When registering a service, you may pass 'kServerOwnsService' as
 * the second parameter to tell the server to delete it when destroyed.
 */
enum ServiceOwnership {
    kServerOwnsService,
    kServerDoesntOwnService
};

class Server {
public:
    /**
     * Create a server with the specified options.
     *
     * If *base is null, the event of listen socket will be
     * added to event base of the last worker thread.
     *
     * Stores a pointer to a heap-allowed server in *srvptr
     * and returns zero on success.
     * Stores NULL in *srvptr and returns an error code on error.
     *
     * Caller should delete *srvptr when it is no longer needed.
     */
    static int New(const ServerOptions &options,
                   event_base *base,
                   Server **srvptr);

    inline Server() { }
    virtual ~Server();

    /**
     * Adds a transport endpoint to the server. 
     *
     * The host specifies either a numerical network address (
     * for IPv4, numbers-and-dots notation;
     * for IPv6, hexadecimal string format),
     * or a network hostname, whose network addresses are looked up.
     *
     * @return
     * Return 0 if success, error code otherwise.
     */
    virtual int Add(const std::string &host, int port) = 0;

    /**
     * Start the server and listen on the options's address.
     *
     * @return
     * Return 0 if success, error code otherwise.
     */
    virtual int Start() = 0;

    /**
     * Stop the server and wait to release all resource.
     *
     * @return
     * Return 0 if success, error code otherwise.
     */
    virtual int Stop() = 0;

    /**
     * Register a service based on protobuf.
     *
     * Service is marked by its fully-qualified name, so each service
     * can be registered only one time.
     *
     * @return
     * Return 0 if success, error code otherwise.
     */
    virtual int Register(google::protobuf::Service *service,
                         ServiceOwnership ownership) = 0;

    /**
     * Unregister a service based on protobuf.
     *
     * The service will be deleted if registered with 'kServerOwnsService'.
     *
     * @return
     * Return 0 if success, error code otherwise.
     */
    virtual int Unregister(std::string service_full_name) = 0;
    virtual int Unregister(google::protobuf::Service *service) = 0;

private:
    /* No copying allowed */
    Server(const Server &);
    void operator=(const Server &);
};

} // namespace qrpc

#endif /* QRPC_RPC_SERVER_H */
