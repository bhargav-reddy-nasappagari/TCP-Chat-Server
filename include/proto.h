#ifndef TCPCHAT_PROTO_H
#define TCPCHAT_PROTO_H

#include <stddef.h>

typedef enum {
    P_EMPTY,
    P_CHAT,
    P_NICK,
    P_WHO,
    P_QUIT,
    P_UNKNOWN,
    P_INVALID
} ptype_t;

typedef struct {
    ptype_t type;
    const char *arg;
} parsed_t;

parsed_t proto_parse(char *line);

int proto_valid_nick(const char *s);

int proto_fmt_chat(
    char *out,
    size_t sz,
    const char *nick,
    const char *text
);

int proto_fmt_notice(
    char *out,
    size_t sz,
    const char *fmt,
    ...
);

int proto_fmt_error(
    char *out,
    size_t sz,
    const char *fmt,
    ...
);

#endif