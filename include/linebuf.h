#ifndef TCPCHAT_LINEBUF_H
#define TCPCHAT_LINEBUF_H

#include <stddef.h>

#include "config.h"

typedef struct {
    char data[LINE_MAX_BYTES];
    size_t len;
} linebuf_t;

typedef enum {
    LB_LINE,
    LB_NEED_MORE,
    LB_TOO_LONG
} lb_status_t;

void lb_init(linebuf_t *lb);

char *lb_space(
    linebuf_t *lb,
    size_t *cap
);

void lb_commit(
    linebuf_t *lb,
    size_t n
);

lb_status_t lb_next(
    linebuf_t *lb,
    char *out,
    size_t outsz
);

#endif