#ifndef TCPCHAT_PROTO_H
#define TCPCHAT_PROTO_H

#include <stddef.h>

#include "config.h"   /* WARN_UNUSED, PRINTF_FMT */

/* Pure logic: no sockets, no threads. */
typedef enum {
    P_EMPTY,     /* blank line: ignore */
    P_CHAT,      /* chat text; arg is the text */
    P_NICK,      /* /nick; arg is the requested nickname */
    P_WHO,       /* /who */
    P_QUIT,      /* /quit */
    P_UNKNOWN,   /* a "/" command that is not recognised */
    P_INVALID    /* control characters present */
} ptype_t;

typedef struct {
    ptype_t type;
    const char *arg;   /* points into the input line; NULL if unused */
} parsed_t;

/* Classifies line. May edit it in place (trims a trailing \r, splits the
 * command from its argument), so arg points into line and is valid only as
 * long as line is. */
parsed_t proto_parse(char *line);

/* Returns 1 if s is 1..NICK_MAX characters from A-Za-z0-9_-, else 0. */
int proto_valid_nick(const char *s) WARN_UNUSED;

/* The three formatters write one complete line ending in \n and never write
 * past sz bytes. Return the length written (excluding the NUL), or -1 if the
 * output did not fit; out is then not to be sent. */

/* "nick: text\n" */
int proto_fmt_chat(
    char *out,
    size_t sz,
    const char *nick,
    const char *text
) WARN_UNUSED;

/* "* <formatted text>\n" */
int proto_fmt_notice(
    char *out,
    size_t sz,
    const char *fmt,
    ...
) PRINTF_FMT(3, 4) WARN_UNUSED;

/* "! <formatted text>\n" */
int proto_fmt_error(
    char *out,
    size_t sz,
    const char *fmt,
    ...
) PRINTF_FMT(3, 4) WARN_UNUSED;

#endif
