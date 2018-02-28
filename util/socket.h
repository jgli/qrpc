#ifndef QRPC_UTIL_SOCKET_H
#define QRPC_UTIL_SOCKET_H

#include <sys/time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/ioctl.h>

#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>
#include <errno.h>
#include <stddef.h>
#include <vector>

#include <netinet/in.h>
#include <netinet/tcp.h>

namespace qrpc {

/* INET_ADDRSTRLEN/INET6_ADDRSTRLEN refer to 'netinet/in.h' */
#define UNIX_ADDRSTRLEN	\
    (sizeof(struct sockaddr_un) - offsetof(struct sockaddr_un, sun_path))

inline int set_blocking(int sd);
inline int set_nonblocking(int sd);
inline int set_reuseaddr(int sd);
inline int set_keepalive(int sd);
inline int set_tcpnodelay(int sd);
inline int set_linger(int sd, int on, int timeout);
inline int set_sndbuf(int sd, int size);
inline int set_rcvbuf(int sd, int size);
inline int get_soerror(int sd);
inline int get_sndbuf(int sd);
inline int get_rcvbuf(int sd);

/*
 * Address resolution for internet (ipv4 and ipv6) and
 * unix domain socket address. 
 */
struct sockinfo {
    int         family;         /* socket address family */
    socklen_t   addrlen;        /* socket address length */

    union {
        struct sockaddr_in	in; /* ipv4 socket address */
        struct sockaddr_in6	in6;/* ipv6 socket address */
        struct sockaddr_un	un; /* unix domain address */
    } addr;
};

/*
 * Resolve a hostname and service by translating it to socket address
 * and return the first address in si.
 *
 * This routine is reentrant
 */
int resolve_addr(const char *host, int port, struct sockinfo *si);

/*
 * Resolve a hostname and service by translating it to socket address
 * and return the addresses in sis.
 *
 * This routine is reentrant
 */
int resolve_addr(const char *host, int port, std::vector<struct sockinfo> &sis);

/*
 * Unresolve the socket address by translating it to a character string
 * describing the host and service.
 *
 * This routine is reentrant
 */
const char* unresolve_addr(struct sockaddr *addr, socklen_t addrlen);

/*
 * Unresolve the socket descriptor peer address by translating it to a
 * character string describing the host and service
 *
 * This routine is reentrant
 */
const char* unresolve_peer_desc(int sd);

/*
 * Unresolve the socket descriptor address by translating it to a
 * character string describing the host and service
 *
 * This routine is reentrant
 */
const char* unresolve_desc(int sd);

inline int set_blocking(int sd)
{
    int flags;
    
    flags = fcntl(sd, F_GETFL, 0);
    if (flags < 0) {
        return flags;
    }
    
    return fcntl(sd, F_SETFL, flags & ~O_NONBLOCK);
}

inline int set_nonblocking(int sd)
{
    int flags;
    
    flags = fcntl(sd, F_GETFL, 0);
    if (flags < 0) {
        return flags;
    }
    
    return fcntl(sd, F_SETFL, flags | O_NONBLOCK);
}

inline int set_reuseaddr(int sd)
{
    int reuse;
    socklen_t len;
    
    reuse = 1;
    len = sizeof(reuse);
    
    return setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &reuse, len);
}

inline int set_keepalive(int sd)
{
    int flags;
    socklen_t len;
    
    flags = 1;
    len = sizeof(flags);
    
    return setsockopt(sd, SOL_SOCKET, SO_KEEPALIVE, &flags, len);
}

/*
 * Disable Nagle algorithm on TCP socket.
 *
 * This option helps to minimize transmit latency by disabling coalescing
 * of data to fill up a TCP segment inside the kernel. Sockets with this
 * option must use readv() or writev() to do data transfer in bulk and
 * hence avoid the overhead of small packets.
 */
inline int set_tcpnodelay(int sd)
{
    int nodelay;
    socklen_t len;
    
    nodelay = 1;
    len = sizeof(nodelay);
    
    return setsockopt(sd, IPPROTO_TCP, TCP_NODELAY, &nodelay, len);
}

inline int set_linger(int sd, int on, int timeout)
{
    struct linger linger;
    socklen_t len;
    
    linger.l_onoff = on;
    linger.l_linger = timeout;
   
    len = sizeof(linger);
    
    return setsockopt(sd, SOL_SOCKET, SO_LINGER, &linger, len);
}

inline int set_sndbuf(int sd, int size)
{
    socklen_t len;
    
    len = sizeof(size);
    
    return setsockopt(sd, SOL_SOCKET, SO_SNDBUF, &size, len);
}

inline int set_rcvbuf(int sd, int size)
{
    socklen_t len;
    
    len = sizeof(size);
    
    return setsockopt(sd, SOL_SOCKET, SO_RCVBUF, &size, len);
}

inline int get_soerror(int sd)
{
    int status, err;
    socklen_t len;
    
    err = 0;
    len = sizeof(err);
    
    status = getsockopt(sd, SOL_SOCKET, SO_ERROR, &err, &len);
    if (status == 0) {
        errno = err;
    }
    
    return status;
}

inline int get_sndbuf(int sd)
{
    int status, size;
    socklen_t len;
    
    size = 0;
    len = sizeof(size);
    
    status = getsockopt(sd, SOL_SOCKET, SO_SNDBUF, &size, &len);
    if (status < 0) {
        return status;
    }
    
    return size;
}

inline int get_rcvbuf(int sd)
{
    int status, size;
    socklen_t len;
    
    size = 0;
    len = sizeof(size);
    
    status = getsockopt(sd, SOL_SOCKET, SO_RCVBUF, &size, &len);
    if (status < 0) {
        return status;
    }
    
    return size;
}

} // namespace qrpc

#endif /* QRPC_UTIL_SOCKET_H */
