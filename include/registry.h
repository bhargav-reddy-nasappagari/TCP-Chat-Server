#ifndef TCPCHAT_REGISTRY_H
#define TCPCHAT_REGISTRY_H

#include <stddef.h>
#include <stdint.h>

#include "config.h"   /* WARN_UNUSED */

/* The shared table of connected clients. It owns the only lock, performs
 * every write to a client socket, and is the only place a client descriptor
 * is closed. Every function locks on entry and unlocks on every return
 * path. Safe to call from any thread; never call it while holding another
 * lock. */

typedef uint32_t client_id_t;   /* never reused during one run; 0 is "none" */

/* Adds a client. On success returns 0, sets *id, and writes the default
 * nickname "user<id>" to nick_out (at least NICK_MAX + 1 bytes); the
 * registry then owns fd. Returns -1 if the table is full, and the caller
 * still owns fd. */
int reg_add(
    int fd,
    const char *ip,
    client_id_t *id,
    char *nick_out
) WARN_UNUSED;

/* Removes the client, closing its descriptor. Returns 1 if this call removed
 * it (and writes its nickname to nick_out, at least NICK_MAX + 1 bytes, or
 * NULL to skip), or 0 if the id was already gone. This is the single cleanup
 * path, which gives exactly-once removal. */
int reg_remove(client_id_t id, char *nick_out) WARN_UNUSED;

/* Changes the nickname if nick is not taken by another client
 * (case-insensitive). Returns 0 on success and writes the previous nickname
 * to old_out (at least NICK_MAX + 1 bytes), or -1 if the name is taken or
 * the id is unknown. Validating the name's syntax is proto's job, not this
 * function's. */
int reg_set_nick(
    client_id_t id,
    const char *nick,
    char *old_out
) WARN_UNUSED;

/* Copies the client's nickname into out (outsz bytes). Returns 0 on success,
 * -1 if the id is unknown. */
int reg_get_nick(
    client_id_t id,
    char *out,
    size_t outsz
) WARN_UNUSED;

/* Writes msg (len bytes) to one client. Returns 0 on success, -1 if the id
 * is unknown or dying, or if the write failed (the client is then marked
 * dying and its socket shut down; its own session thread cleans it up). */
int reg_send_to(
    client_id_t id,
    const char *msg,
    size_t len
) WARN_UNUSED;

/* Writes msg to every live client except from (from == 0 means everyone).
 * The whole broadcast happens in one critical section, so all clients see
 * broadcasts in the same order. Failed recipients are marked dying.
 * Returns the number of clients the message was delivered to. */
int reg_broadcast(
    client_id_t from,
    const char *msg,
    size_t len
) WARN_UNUSED;

/* Writes the connected nicknames, comma-separated, into out (sz bytes,
 * always NUL-terminated, truncated if too small). Returns the number of
 * clients listed, or -1 if sz is 0. */
int reg_who(
    char *out,
    size_t sz
) WARN_UNUSED;

/* Calls shutdown(fd, SHUT_RDWR) on every client so each session thread wakes
 * from recv. Does not close or remove anything. */
void reg_kick_all(void);

/* Number of connected clients. */
size_t reg_count(void) WARN_UNUSED;

#endif
