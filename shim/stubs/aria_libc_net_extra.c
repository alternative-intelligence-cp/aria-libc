// Extra net shim functions
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <unistd.h>
#include <stdint.h>

int64_t aria_libc_net_socket_tcp(void) {
    return (int64_t)socket(AF_INET, SOCK_STREAM, 0);
}

int aria_libc_net_connect(int64_t fd, const char* host, int64_t port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    inet_pton(AF_INET, host, &addr.sin_addr);
    return connect((int)fd, (struct sockaddr*)&addr, sizeof(addr));
}

int aria_libc_net_bind(int64_t fd, const char* host, int64_t port) {
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((uint16_t)port);
    if (host) inet_pton(AF_INET, host, &addr.sin_addr);
    else addr.sin_addr.s_addr = INADDR_ANY;
    return bind((int)fd, (struct sockaddr*)&addr, sizeof(addr));
}

int64_t aria_libc_net_recv(int64_t fd, void* buf, int64_t len) {
    return (int64_t)recv((int)fd, buf, (size_t)len, 0);
}

int64_t aria_libc_net_send_str(int64_t fd, const char* str) {
    size_t len = strlen(str);
    return (int64_t)send((int)fd, str, len, 0);
}

int aria_libc_net_setsockopt_reuse(int64_t fd) {
    int opt = 1;
    return setsockopt((int)fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
}
