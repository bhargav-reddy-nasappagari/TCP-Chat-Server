#ifndef TCPCHAT_LINEBUF_H
#define TCPCHAT_LINEBUF_H

#include <stddef.h>

#include "config.h"   /* LINE_MAX_BYTES, WARN_UNUSED */

/* Pure framing logic: no I/O, no allocation, no locks. */
typedef struct {
    char data[LINE_MAX_BYTES];
    size_t len;
} linebuf_t;

typedef enum {
    LB_LINE,       /* one complete line was copied to out */
    LB_NEED_MORE,  /* no complete line yet; receive more bytes */
    LB_TOO_LONG    /* buffer full with no newline; framing is lost */
} lb_status_t;

void lb_init(linebuf_t *lb);

/* Returns where recv should write and sets *cap to the free bytes there.
 * *cap is 0 when the buffer is full. */
char *lb_space(
    linebuf_t *lb,
    size_t *cap
);

/* Records that n bytes were just written at the pointer lb_space returned.
 * n must not exceed the *cap that call reported. */
void lb_commit(
    linebuf_t *lb,
    size_t n
);

/* Extracts one line into out (NUL-terminated, without the \n or a preceding
 * \r) and shifts any remaining bytes to the front. outsz should be at least
 * LINE_MAX_BYTES. Call repeatedly until it returns LB_NEED_MORE or
 * LB_TOO_LONG. */
lb_status_t lb_next(
    linebuf_t *lb,
    char *out,
    size_t outsz
) WARN_UNUSED;

#endif
