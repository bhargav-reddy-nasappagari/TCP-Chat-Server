#ifndef TCPCHAT_REGISTRY_H
#define TCPCHAT_REGISTRY_H

#include <stddef.h>
#include <stdint.h>

typedef uint32_t client_id_t;

int reg_init(void);

int reg_add(
    int fd,
    const char *ip,
    client_id_t *id,
    char *nick_out
);

int reg_remove(client_id_t id, char *nick_out);

int reg_set_nick(
    client_id_t id,
    const char *nick,
    char *old_out
);

int reg_get_nick(
    client_id_t id,
    char *out,
    size_t outsz
);

int reg_send_to(
    client_id_t id,
    const char *msg,
    size_t len
);

int reg_broadcast(
    client_id_t from,
    const char *msg,
    size_t len
);

int reg_who(
    char *out,
    size_t sz
);

void reg_kick_all(void);

size_t reg_count(void);

#endif
