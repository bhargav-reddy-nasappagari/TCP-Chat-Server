#ifndef TCPCHAT_NET_H
#define TCPCHAT_NET_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

int net_listen(uint16_t port, int backlog);

int net_accept(
    int listen_fd,
    char *ip,
    size_t ipsz,
    uint16_t *port
);

int net_set_send_timeout(int fd, int ms);

ssize_t net_send_all(
    int fd,
    const char *buf,
    size_t len
);

ssize_t net_recv(
    int fd,
    char *buf,
    size_t cap
);

#endif