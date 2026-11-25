#ifndef TCPCHAT_SESSION_H
#define TCPCHAT_SESSION_H

typedef struct {
    int fd;
    char ip[46];
} session_arg_t;

void session_run(session_arg_t *arg);

#endif
