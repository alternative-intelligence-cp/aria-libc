/*  aria_libc_net.c — Flat-parameter C bridge between Aria FFI and BSD sockets
 *
 *  Build (dynamic): cc -O2 -shared -fPIC -Wall -o libaria_libc_net.so aria_libc_net.c
 *  Build (static):  cc -O2 -c -Wall -o aria_libc_net.o aria_libc_net.c
 *                   ar rcs libaria_libc_net.a aria_libc_net.o
 *
 *  Aria ABI rules:
 *    - String params  = const char* (null-terminated)
 *    - String returns = AriaString {char* data, int64_t length} by value
 *    - flt32/flt64    = double at C ABI
 *    - "free" in names = shadowed — use _release/_destroy/_cleanup
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>

/* ── Aria string return type ─────────────────────────────────────────── */

typedef struct {
    char    *data;
    int64_t  length;
} AriaString;

static AriaString make_aria_string(const char *s) {
    if (!s) {
        AriaString r = { NULL, 0 };
        return r;
    }
    int64_t len = (int64_t)strlen(s);
    char *copy = (char *)malloc(len + 1);
    if (copy) {
        memcpy(copy, s, len + 1);
    }
    AriaString r = { copy, len };
    return r;
}

/* ── Error state ─────────────────────────────────────────────────────── */

static int last_errno = 0;

int64_t aria_libc_net_errno(void) {
    return (int64_t)last_errno;
}

AriaString aria_libc_net_strerror(int64_t errnum) {
    return make_aria_string(strerror((int)errnum));
}

/* ── Socket constants (as functions, Aria ABI) ───────────────────────── */

/* Address families */
int64_t aria_libc_net_AF_INET(void)     { return AF_INET; }
int64_t aria_libc_net_AF_INET6(void)    { return AF_INET6; }
int64_t aria_libc_net_AF_UNIX(void)     { return AF_UNIX; }
int64_t aria_libc_net_AF_UNSPEC(void)   { return AF_UNSPEC; }

/* Socket types */
int64_t aria_libc_net_SOCK_STREAM(void) { return SOCK_STREAM; }
int64_t aria_libc_net_SOCK_DGRAM(void)  { return SOCK_DGRAM; }
int64_t aria_libc_net_SOCK_RAW(void)    { return SOCK_RAW; }

/* Protocol constants */
int64_t aria_libc_net_IPPROTO_TCP(void) { return IPPROTO_TCP; }
int64_t aria_libc_net_IPPROTO_UDP(void) { return IPPROTO_UDP; }

/* Socket option levels */
int64_t aria_libc_net_SOL_SOCKET(void)  { return SOL_SOCKET; }

/* Socket options */
int64_t aria_libc_net_SO_REUSEADDR(void) { return SO_REUSEADDR; }
int64_t aria_libc_net_SO_REUSEPORT(void) { return SO_REUSEPORT; }
int64_t aria_libc_net_SO_KEEPALIVE(void) { return SO_KEEPALIVE; }
int64_t aria_libc_net_SO_RCVBUF(void)    { return SO_RCVBUF; }
int64_t aria_libc_net_SO_SNDBUF(void)    { return SO_SNDBUF; }
int64_t aria_libc_net_SO_ERROR(void)     { return SO_ERROR; }
int64_t aria_libc_net_TCP_NODELAY(void)  { return TCP_NODELAY; }

/* Shutdown modes */
int64_t aria_libc_net_SHUT_RD(void)   { return SHUT_RD; }
int64_t aria_libc_net_SHUT_WR(void)   { return SHUT_WR; }
int64_t aria_libc_net_SHUT_RDWR(void) { return SHUT_RDWR; }

/* Poll event flags */
int64_t aria_libc_net_POLLIN(void)   { return POLLIN; }
int64_t aria_libc_net_POLLOUT(void)  { return POLLOUT; }
int64_t aria_libc_net_POLLERR(void)  { return POLLERR; }
int64_t aria_libc_net_POLLHUP(void)  { return POLLHUP; }

/* ── Core socket operations ──────────────────────────────────────────── */

int64_t aria_libc_net_socket(int64_t domain, int64_t type, int64_t protocol) {
    int fd = socket((int)domain, (int)type, (int)protocol);
    if (fd < 0) { last_errno = errno; return -1; }
    return (int64_t)fd;
}

int64_t aria_libc_net_close(int64_t fd) {
    int rc = close((int)fd);
    if (rc < 0) { last_errno = errno; return -1; }
    return 0;
}

int64_t aria_libc_net_shutdown(int64_t fd, int64_t how) {
    int rc = shutdown((int)fd, (int)how);
    if (rc < 0) { last_errno = errno; return -1; }
    return 0;
}

/* ── Bind / Listen / Accept (server side) ────────────────────────────── */

int64_t aria_libc_net_bind_ipv4(int64_t fd, const char *addr, int64_t port) {
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons((uint16_t)port);

    if (addr == NULL || strlen(addr) == 0 || strcmp(addr, "0.0.0.0") == 0) {
        sa.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (inet_pton(AF_INET, addr, &sa.sin_addr) != 1) {
            last_errno = EINVAL;
            return -1;
        }
    }

    int rc = bind((int)fd, (struct sockaddr *)&sa, sizeof(sa));
    if (rc < 0) { last_errno = errno; return -1; }
    return 0;
}

int64_t aria_libc_net_bind_ipv6(int64_t fd, const char *addr, int64_t port) {
    struct sockaddr_in6 sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin6_family = AF_INET6;
    sa.sin6_port   = htons((uint16_t)port);

    if (addr == NULL || strlen(addr) == 0 || strcmp(addr, "::") == 0) {
        sa.sin6_addr = in6addr_any;
    } else {
        if (inet_pton(AF_INET6, addr, &sa.sin6_addr) != 1) {
            last_errno = EINVAL;
            return -1;
        }
    }

    int rc = bind((int)fd, (struct sockaddr *)&sa, sizeof(sa));
    if (rc < 0) { last_errno = errno; return -1; }
    return 0;
}

int64_t aria_libc_net_listen(int64_t fd, int64_t backlog) {
    int rc = listen((int)fd, (int)backlog);
    if (rc < 0) { last_errno = errno; return -1; }
    return 0;
}

/* Accept returns the new client fd, or -1 on error.
 * Client address info retrieved via aria_libc_net_peer_addr(). */
int64_t aria_libc_net_accept(int64_t fd) {
    struct sockaddr_storage sa;
    socklen_t len = sizeof(sa);
    int cfd = accept((int)fd, (struct sockaddr *)&sa, &len);
    if (cfd < 0) { last_errno = errno; return -1; }
    return (int64_t)cfd;
}

/* ── Connect (client side) ───────────────────────────────────────────── */

int64_t aria_libc_net_connect_ipv4(int64_t fd, const char *addr, int64_t port) {
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons((uint16_t)port);

    if (inet_pton(AF_INET, addr, &sa.sin_addr) != 1) {
        last_errno = EINVAL;
        return -1;
    }

    int rc = connect((int)fd, (struct sockaddr *)&sa, sizeof(sa));
    if (rc < 0) { last_errno = errno; return -1; }
    return 0;
}

int64_t aria_libc_net_connect_ipv6(int64_t fd, const char *addr, int64_t port) {
    struct sockaddr_in6 sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin6_family = AF_INET6;
    sa.sin6_port   = htons((uint16_t)port);

    if (inet_pton(AF_INET6, addr, &sa.sin6_addr) != 1) {
        last_errno = EINVAL;
        return -1;
    }

    int rc = connect((int)fd, (struct sockaddr *)&sa, sizeof(sa));
    if (rc < 0) { last_errno = errno; return -1; }
    return 0;
}

/* ── Send / Recv ─────────────────────────────────────────────────────── */

int64_t aria_libc_net_send_string(int64_t fd, const char *data) {
    if (!data) { last_errno = EINVAL; return -1; }
    size_t len = strlen(data);
    ssize_t n = send((int)fd, data, len, 0);
    if (n < 0) { last_errno = errno; return -1; }
    return (int64_t)n;
}

int64_t aria_libc_net_send_bytes(int64_t fd, const char *data, int64_t length) {
    if (!data || length < 0) { last_errno = EINVAL; return -1; }
    ssize_t n = send((int)fd, data, (size_t)length, 0);
    if (n < 0) { last_errno = errno; return -1; }
    return (int64_t)n;
}

/* Receive up to max_len bytes, returned as AriaString */
AriaString aria_libc_net_recv_string(int64_t fd, int64_t max_len) {
    if (max_len <= 0) max_len = 4096;
    char *buf = (char *)malloc((size_t)max_len + 1);
    if (!buf) {
        last_errno = ENOMEM;
        AriaString r = { NULL, 0 };
        return r;
    }
    ssize_t n = recv((int)fd, buf, (size_t)max_len, 0);
    if (n < 0) {
        last_errno = errno;
        free(buf);
        AriaString r = { NULL, 0 };
        return r;
    }
    buf[n] = '\0';
    AriaString r = { buf, (int64_t)n };
    return r;
}

/* Receive, returning the byte count (data written into internal buffer) */
int64_t aria_libc_net_recv_bytes(int64_t fd, int64_t max_len) {
    /* We return just the count — use recv_string for actual data */
    if (max_len <= 0) max_len = 4096;
    char *buf = (char *)malloc((size_t)max_len);
    if (!buf) { last_errno = ENOMEM; return -1; }
    ssize_t n = recv((int)fd, buf, (size_t)max_len, 0);
    if (n < 0) { last_errno = errno; free(buf); return -1; }
    free(buf);
    return (int64_t)n;
}

/* ── UDP: sendto / recvfrom ──────────────────────────────────────────── */

int64_t aria_libc_net_sendto_ipv4(int64_t fd, const char *data, const char *addr, int64_t port) {
    if (!data || !addr) { last_errno = EINVAL; return -1; }
    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons((uint16_t)port);
    if (inet_pton(AF_INET, addr, &sa.sin_addr) != 1) {
        last_errno = EINVAL;
        return -1;
    }
    size_t len = strlen(data);
    ssize_t n = sendto((int)fd, data, len, 0, (struct sockaddr *)&sa, sizeof(sa));
    if (n < 0) { last_errno = errno; return -1; }
    return (int64_t)n;
}

AriaString aria_libc_net_recvfrom_ipv4(int64_t fd, int64_t max_len) {
    if (max_len <= 0) max_len = 4096;
    char *buf = (char *)malloc((size_t)max_len + 1);
    if (!buf) {
        last_errno = ENOMEM;
        AriaString r = { NULL, 0 };
        return r;
    }
    struct sockaddr_in sa;
    socklen_t slen = sizeof(sa);
    ssize_t n = recvfrom((int)fd, buf, (size_t)max_len, 0, (struct sockaddr *)&sa, &slen);
    if (n < 0) {
        last_errno = errno;
        free(buf);
        AriaString r = { NULL, 0 };
        return r;
    }
    buf[n] = '\0';
    AriaString r = { buf, (int64_t)n };
    return r;
}

/* ── Socket options ──────────────────────────────────────────────────── */

int64_t aria_libc_net_setsockopt_int(int64_t fd, int64_t level, int64_t optname, int64_t val) {
    int v = (int)val;
    int rc = setsockopt((int)fd, (int)level, (int)optname, &v, sizeof(v));
    if (rc < 0) { last_errno = errno; return -1; }
    return 0;
}

int64_t aria_libc_net_getsockopt_int(int64_t fd, int64_t level, int64_t optname) {
    int v = 0;
    socklen_t len = sizeof(v);
    int rc = getsockopt((int)fd, (int)level, (int)optname, &v, &len);
    if (rc < 0) { last_errno = errno; return -1; }
    return (int64_t)v;
}

/* ── Non-blocking / fcntl helpers ────────────────────────────────────── */

int64_t aria_libc_net_set_nonblocking(int64_t fd, int64_t enabled) {
    int flags = fcntl((int)fd, F_GETFL, 0);
    if (flags < 0) { last_errno = errno; return -1; }
    if (enabled) {
        flags |= O_NONBLOCK;
    } else {
        flags &= ~O_NONBLOCK;
    }
    int rc = fcntl((int)fd, F_SETFL, flags);
    if (rc < 0) { last_errno = errno; return -1; }
    return 0;
}

int64_t aria_libc_net_is_nonblocking(int64_t fd) {
    int flags = fcntl((int)fd, F_GETFL, 0);
    if (flags < 0) { last_errno = errno; return -1; }
    return (flags & O_NONBLOCK) ? 1 : 0;
}

/* ── Poll (simple single-fd) ────────────────────────────────────────── */

int64_t aria_libc_net_poll_read(int64_t fd, int64_t timeout_ms) {
    struct pollfd pfd;
    pfd.fd = (int)fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    int rc = poll(&pfd, 1, (int)timeout_ms);
    if (rc < 0) { last_errno = errno; return -1; }
    if (rc == 0) return 0; /* timeout */
    return (int64_t)pfd.revents;
}

int64_t aria_libc_net_poll_write(int64_t fd, int64_t timeout_ms) {
    struct pollfd pfd;
    pfd.fd = (int)fd;
    pfd.events = POLLOUT;
    pfd.revents = 0;
    int rc = poll(&pfd, 1, (int)timeout_ms);
    if (rc < 0) { last_errno = errno; return -1; }
    if (rc == 0) return 0;
    return (int64_t)pfd.revents;
}

/* ── Address resolution (getaddrinfo) ────────────────────────────────── */

/* Resolve a hostname to its first IPv4 address string */
AriaString aria_libc_net_resolve_ipv4(const char *hostname) {
    if (!hostname) return make_aria_string(NULL);

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(hostname, NULL, &hints, &res);
    if (rc != 0 || !res) {
        last_errno = ENOENT;
        return make_aria_string(NULL);
    }

    char ip[INET_ADDRSTRLEN];
    struct sockaddr_in *sa4 = (struct sockaddr_in *)res->ai_addr;
    inet_ntop(AF_INET, &sa4->sin_addr, ip, sizeof(ip));
    freeaddrinfo(res);
    return make_aria_string(ip);
}

/* Resolve a hostname to its first IPv6 address string */
AriaString aria_libc_net_resolve_ipv6(const char *hostname) {
    if (!hostname) return make_aria_string(NULL);

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_INET6;
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(hostname, NULL, &hints, &res);
    if (rc != 0 || !res) {
        last_errno = ENOENT;
        return make_aria_string(NULL);
    }

    char ip[INET6_ADDRSTRLEN];
    struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)res->ai_addr;
    inet_ntop(AF_INET6, &sa6->sin6_addr, ip, sizeof(ip));
    freeaddrinfo(res);
    return make_aria_string(ip);
}

/* Resolve hostname, return first address (any family) */
AriaString aria_libc_net_resolve(const char *hostname) {
    if (!hostname) return make_aria_string(NULL);

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int rc = getaddrinfo(hostname, NULL, &hints, &res);
    if (rc != 0 || !res) {
        last_errno = ENOENT;
        return make_aria_string(NULL);
    }

    char ip[INET6_ADDRSTRLEN];  /* large enough for both */
    if (res->ai_family == AF_INET) {
        struct sockaddr_in *sa4 = (struct sockaddr_in *)res->ai_addr;
        inet_ntop(AF_INET, &sa4->sin_addr, ip, sizeof(ip));
    } else {
        struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)res->ai_addr;
        inet_ntop(AF_INET6, &sa6->sin6_addr, ip, sizeof(ip));
    }
    freeaddrinfo(res);
    return make_aria_string(ip);
}

/* ── Peer/local address info ─────────────────────────────────────────── */

AriaString aria_libc_net_peer_addr(int64_t fd) {
    struct sockaddr_storage sa;
    socklen_t len = sizeof(sa);
    if (getpeername((int)fd, (struct sockaddr *)&sa, &len) < 0) {
        last_errno = errno;
        return make_aria_string(NULL);
    }
    char ip[INET6_ADDRSTRLEN];
    if (sa.ss_family == AF_INET) {
        struct sockaddr_in *s4 = (struct sockaddr_in *)&sa;
        inet_ntop(AF_INET, &s4->sin_addr, ip, sizeof(ip));
    } else {
        struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)&sa;
        inet_ntop(AF_INET6, &s6->sin6_addr, ip, sizeof(ip));
    }
    return make_aria_string(ip);
}

int64_t aria_libc_net_peer_port(int64_t fd) {
    struct sockaddr_storage sa;
    socklen_t len = sizeof(sa);
    if (getpeername((int)fd, (struct sockaddr *)&sa, &len) < 0) {
        last_errno = errno;
        return -1;
    }
    if (sa.ss_family == AF_INET) {
        struct sockaddr_in *s4 = (struct sockaddr_in *)&sa;
        return (int64_t)ntohs(s4->sin_port);
    } else {
        struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)&sa;
        return (int64_t)ntohs(s6->sin6_port);
    }
}

AriaString aria_libc_net_local_addr(int64_t fd) {
    struct sockaddr_storage sa;
    socklen_t len = sizeof(sa);
    if (getsockname((int)fd, (struct sockaddr *)&sa, &len) < 0) {
        last_errno = errno;
        return make_aria_string(NULL);
    }
    char ip[INET6_ADDRSTRLEN];
    if (sa.ss_family == AF_INET) {
        struct sockaddr_in *s4 = (struct sockaddr_in *)&sa;
        inet_ntop(AF_INET, &s4->sin_addr, ip, sizeof(ip));
    } else {
        struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)&sa;
        inet_ntop(AF_INET6, &s6->sin6_addr, ip, sizeof(ip));
    }
    return make_aria_string(ip);
}

int64_t aria_libc_net_local_port(int64_t fd) {
    struct sockaddr_storage sa;
    socklen_t len = sizeof(sa);
    if (getsockname((int)fd, (struct sockaddr *)&sa, &len) < 0) {
        last_errno = errno;
        return -1;
    }
    if (sa.ss_family == AF_INET) {
        struct sockaddr_in *s4 = (struct sockaddr_in *)&sa;
        return (int64_t)ntohs(s4->sin_port);
    } else {
        struct sockaddr_in6 *s6 = (struct sockaddr_in6 *)&sa;
        return (int64_t)ntohs(s6->sin6_port);
    }
}

/* ── Convenience: TCP connect + send/recv in one call ────────────────── */

/* Connect to host:port, return fd or -1 */
int64_t aria_libc_net_tcp_connect(const char *host, int64_t port) {
    if (!host) { last_errno = EINVAL; return -1; }

    struct addrinfo hints, *res = NULL;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family   = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", (int)port);

    int rc = getaddrinfo(host, port_str, &hints, &res);
    if (rc != 0 || !res) {
        last_errno = ENOENT;
        return -1;
    }

    int fd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (fd < 0) {
        last_errno = errno;
        freeaddrinfo(res);
        return -1;
    }

    if (connect(fd, res->ai_addr, res->ai_addrlen) < 0) {
        last_errno = errno;
        close(fd);
        freeaddrinfo(res);
        return -1;
    }

    freeaddrinfo(res);
    return (int64_t)fd;
}

/* Create TCP server socket, bind, listen. Returns fd or -1. */
int64_t aria_libc_net_tcp_listen(const char *addr, int64_t port, int64_t backlog) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) { last_errno = errno; return -1; }

    /* SO_REUSEADDR to avoid "address already in use" */
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    struct sockaddr_in sa;
    memset(&sa, 0, sizeof(sa));
    sa.sin_family = AF_INET;
    sa.sin_port   = htons((uint16_t)port);

    if (addr == NULL || strlen(addr) == 0 || strcmp(addr, "0.0.0.0") == 0) {
        sa.sin_addr.s_addr = htonl(INADDR_ANY);
    } else {
        if (inet_pton(AF_INET, addr, &sa.sin_addr) != 1) {
            last_errno = EINVAL;
            close(fd);
            return -1;
        }
    }

    if (bind(fd, (struct sockaddr *)&sa, sizeof(sa)) < 0) {
        last_errno = errno;
        close(fd);
        return -1;
    }

    if (listen(fd, (int)backlog) < 0) {
        last_errno = errno;
        close(fd);
        return -1;
    }

    return (int64_t)fd;
}

/* ── Hostname/IP utilities ───────────────────────────────────────────── */

AriaString aria_libc_net_gethostname(void) {
    char buf[256];
    if (gethostname(buf, sizeof(buf)) < 0) {
        last_errno = errno;
        return make_aria_string(NULL);
    }
    buf[sizeof(buf) - 1] = '\0';
    return make_aria_string(buf);
}

/* Convert IPv4 dotted notation to 32-bit integer (network byte order) */
int64_t aria_libc_net_inet_aton(const char *addr) {
    if (!addr) { last_errno = EINVAL; return -1; }
    struct in_addr ia;
    if (inet_pton(AF_INET, addr, &ia) != 1) {
        last_errno = EINVAL;
        return -1;
    }
    return (int64_t)ntohl(ia.s_addr);
}

/* Convert 32-bit integer to IPv4 dotted notation */
AriaString aria_libc_net_inet_ntoa(int64_t ip) {
    struct in_addr ia;
    ia.s_addr = htonl((uint32_t)ip);
    char buf[INET_ADDRSTRLEN];
    inet_ntop(AF_INET, &ia, buf, sizeof(buf));
    return make_aria_string(buf);
}

/* Network byte order conversions */
int64_t aria_libc_net_htons(int64_t val) { return (int64_t)htons((uint16_t)val); }
int64_t aria_libc_net_ntohs(int64_t val) { return (int64_t)ntohs((uint16_t)val); }
int64_t aria_libc_net_htonl(int64_t val) { return (int64_t)htonl((uint32_t)val); }
int64_t aria_libc_net_ntohl(int64_t val) { return (int64_t)ntohl((uint32_t)val); }
