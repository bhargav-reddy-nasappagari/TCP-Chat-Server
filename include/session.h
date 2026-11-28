#ifndef TCPCHAT_SESSION_H
#define TCPCHAT_SESSION_H

#include "config.h"   /* IP_STR_MAX */

/* Allocated by the server, freed by session_run. */
typedef struct {
    int fd;
    char ip[IP_STR_MAX];
} session_arg_t;

/* Runs one client from first byte to removal, in the calling thread: register,
 * welcome, announce the join, read loop, then reg_remove once and announce
 * the leave. Takes ownership of arg and frees it before returning. */
void session_run(session_arg_t *arg);

#endif
