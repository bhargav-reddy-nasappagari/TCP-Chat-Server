#ifndef TCPCHAT_CONFIG_H
#define TCPCHAT_CONFIG_H

#define MAX_CLIENTS       64
#define LINE_MAX_BYTES    512
#define NICK_MAX          16
#define SEND_TIMEOUT_MS   1000
#define LISTEN_BACKLOG    16
#define SHUTDOWN_WAIT_S   5
#define SESSION_STACK     (256 * 1024)
#define OUT_MAX       (LINE_MAX_BYTES + NICK_MAX + 8)
#define DEFAULT_PORT  5000

#endif
