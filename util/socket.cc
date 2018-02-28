#include <sys/types.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <netdb.h>
#include <netinet/in.h>

#include "src/qrpc/util/log.h"
#include "src/qrpc/util/socket.h"

using namespace std;

namespace qrpc {

static int resolve_inet(const char *host, int port, struct sockinfo *si)
{
    int status;
    struct addrinfo *ai, *cai;  // head and current addrinfo
    struct addrinfo hints;
    char *node, service[NI_MAXSERV];
    bool found;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_NUMERICSERV;
    hints.ai_family = AF_UNSPEC;    // AF_INET or AF_INET6
    hints.ai_socktype = SOCK_STREAM;
    
    if (host != NULL) {
        node = (char *)host;
    } else {
        /*
         * If AI_PASSIVE flag is specified in hints.ai_flags,
         * and node is NULL, then the returned socket addresses
         * will be suitable for bind(2)ing a socket that will 
         * accept(2) connections. The returned socket address
         * will contain the wildcard IP address.
         */
        node = NULL;
        hints.ai_flags |= AI_PASSIVE;
    }
    
    snprintf(service, NI_MAXSERV, "%d", port);
    
    status = getaddrinfo(node, service, &hints, &ai);
    if (status < 0) {
        LOG(ERROR) << "address resolution of node: " << node
            << ", service: " << service << " failed: "
            << gai_strerror(status);
        return -1;
    }
    
    /*
     * getaddrinfo() can return a linked list of more than one addrinfo,
     * since we requested for both AF_INET and AF_INET6 addresses and the
     * host itself can be multi-homed. Since we don't care whether we are
     * using ipv4 or ipv6, we just use the first address from this collection
     * in the order in which it was returned.
     *
     * The sorting function used within getaddrinfo() is defined in RFC 3484;
     * the order can be tweaked for a particular system by editing
     * /etc/gai.conf
     */
    for (cai = ai, found = false; cai != NULL; cai = cai->ai_next) {
        si->family = cai->ai_family;
        si->addrlen = cai->ai_addrlen;
        memcpy(&si->addr, cai->ai_addr, si->addrlen);
        
        found = true;
        break;
    }
    
    freeaddrinfo(ai);
    
    return !found ? -1 : 0;
}

static int resolve_inet(const char *host, int port, vector<sockinfo> &sis)
{
    int status;
    struct addrinfo *ai, *cai;  // head and current addrinfo
    struct addrinfo hints;
    char *node, service[NI_MAXSERV];
    bool found;
    
    memset(&hints, 0, sizeof(hints));
    hints.ai_flags = AI_NUMERICSERV;
    hints.ai_family = AF_UNSPEC;    // AF_INET or AF_INET6
    hints.ai_socktype = SOCK_STREAM;
    
    if (host != NULL) {
        node = (char *)host;
    } else {
        /*
         * If AI_PASSIVE flag is specified in hints.ai_flags,
         * and node is NULL, then the returned socket addresses
         * will be suitable for bind(2)ing a socket that will 
         * accept(2) connections. The returned socket address
         * will contain the wildcard IP address.
         */
        node = NULL;
        hints.ai_flags |= AI_PASSIVE;
    }
    
    snprintf(service, NI_MAXSERV, "%d", port);
    
    status = getaddrinfo(node, service, &hints, &ai);
    if (status < 0) {
        LOG(ERROR) << "address resolution of node: " << node
            << ", service: " << service << " failed: "
            << gai_strerror(status);
        return -1;
    }
    
    /*
     * getaddrinfo() can return a linked list of more than one addrinfo,
     * since we requested for both AF_INET and AF_INET6 addresses and the
     * host itself can be multi-homed. Since we don't care whether we are
     * using ipv4 or ipv6, we just use the addresses from this collection
     * in the order in which it was returned.
     *
     * The sorting function used within getaddrinfo() is defined in RFC 3484;
     * the order can be tweaked for a particular system by editing
     * /etc/gai.conf
     */
    for (cai = ai, found = false; cai != NULL; cai = cai->ai_next) {
        struct sockinfo si;
        si.family = cai->ai_family;
        si.addrlen = cai->ai_addrlen;
        memcpy(&si.addr, cai->ai_addr, si.addrlen);

        found = true;
        sis.push_back(si);
    }
    
    freeaddrinfo(ai);
    
    return !found ? -1 : 0;
}

static int resolve_unix(const char *host, struct sockinfo *si)
{
    size_t len;
    struct sockaddr_un *un;
    
    len = strlen(host);
    
    if (len >= UNIX_ADDRSTRLEN) {
        return -1;
    }
    
    un = &si->addr.un;
    
    un->sun_family = AF_UNIX;
    memcpy(un->sun_path, host, len);
    un->sun_path[len] = '\0';
    
    si->family = AF_UNIX;
    si->addrlen = sizeof(*un);
    /* si->addr is an alias of un */
    
    return 0;
}

/*
 * Resolve a hostname and service by translating it to socket address
 * and return it in si
 *
 * This routine is reentrant
 */
int resolve_addr(const char *host, int port, struct sockinfo *si)
{
    if (host != NULL && host[0] == '/') {
        return resolve_unix(host, si);
    }
    
    return resolve_inet(host, port, si);
}

/*
 * Resolve a hostname and service by translating it to socket address
 * and return the addresses in sis.
 *
 * This routine is reentrant
 */
int resolve_addr(const char *host, int port, std::vector<sockinfo> &sis)
{
    sis.clear();

    if (host != NULL && host[0] == '/') {
        struct sockinfo si;
        int rc = resolve_unix(host, &si);
        if (!rc) {
            sis.push_back(si);
        }
        return rc;
    }

    return resolve_inet(host, port, sis);
}

/*
 * Unresolve the socket address by translating it to a character string
 * describing the host and service
 *
 * This routine is reentrant
 */
const char* unresolve_addr(struct sockaddr *addr, socklen_t addrlen)
{
    static __thread char unresolve[NI_MAXHOST + NI_MAXSERV];
    static __thread char host[NI_MAXHOST], service[NI_MAXSERV];
    
    int status;
    
    status = getnameinfo(addr, addrlen, host, sizeof(host),
                         service, sizeof(service),
                         NI_NUMERICHOST | NI_NUMERICSERV);
    if (status < 0) {
        return "unknown";
    }
    
    snprintf(unresolve, sizeof(unresolve), "%s:%s", host, service);
    
    return unresolve;
}

/*
 * Unresolve the socket descriptor peer address by translating it to a
 * character string describing the host and service
 *
 * This routine is reentrant
 */
const char* unresolve_peer_desc(int sd)
{
    struct sockinfo si;
    
    int status;
    socklen_t addrlen;
    struct sockaddr *addr;
    
    memset(&si, 0, sizeof(si));
    addr = (struct sockaddr *)&si.addr;
    addrlen = sizeof(si.addr);
    
    status = getpeername(sd, addr, &addrlen);
    if (status < 0) {
        return "unknown";
    }
    
    return unresolve_addr(addr, addrlen);
}

/*
 * Unresolve the socket descriptor address by translating it to a
 * character string describing the host and service
 *
 * This routine is reentrant
 */
const char* unresolve_desc(int sd)
{
    struct sockinfo si;
    
    int status;
    socklen_t addrlen;
    struct sockaddr *addr;
    
    memset(&si, 0, sizeof(si));
    addr = (struct sockaddr *)&si.addr;
    addrlen = sizeof(si.addr);
    
    status = getsockname(sd, addr, &addrlen);
    if (status < 0) {
        return "unknown";
    }
    
    return unresolve_addr(addr, addrlen);
}

} // namespace qrpc
