#ifndef TCPCHAT_NET_H
#define TCPCHAT_NET_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#include "config.h"   /* WARN_UNUSED */

/* Creates a listening IPv4 TCP socket on port (SO_REUSEADDR, bind, listen).
 * Returns the descriptor, or -1 on any failure; nothing is left open on
 * failure. */
int net_listen(uint16_t port, int backlog) WARN_UNUSED;

/* Waits up to 500 ms for a connection, retrying on EINTR so the caller can
 * check g_stop regularly. On success fills ip (ipsz bytes, use IP_STR_MAX)
 * and *port with the peer address and enables SO_KEEPALIVE.
 * Returns the new descriptor, 0 on timeout, or -1 on error (errno set). */
int net_accept(
    int listen_fd,
    char *ip,
    size_t ipsz,
    uint16_t *port
) WARN_UNUSED;

/* Sets SO_SNDTIMEO to ms milliseconds. Returns 0 on success, -1 on error. */
int net_set_send_timeout(int fd, int ms) WARN_UNUSED;

/* Sends all len bytes, looping over partial writes and retrying on EINTR.
 * Uses MSG_NOSIGNAL, so a dead peer yields EPIPE rather than SIGPIPE.
 * Returns len on success, or -1 on any other error, including a send
 * timeout (EAGAIN). */
ssize_t net_send_all(
    int fd,
    const char *buf,
    size_t len
) WARN_UNUSED;

/* One recv of up to cap bytes, retried on EINTR.
 * Returns >0 bytes read, 0 if the peer closed in an orderly way, or -1 on
 * error. */
ssize_t net_recv(
    int fd,
    char *buf,
    size_t cap
) WARN_UNUSED;

#endif
