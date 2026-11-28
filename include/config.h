#ifndef TCPCHAT_CONFIG_H
#define TCPCHAT_CONFIG_H

/* Limits. Every module uses these, so buffer sizes always agree. */
#define MAX_CLIENTS       64
#define LINE_MAX_BYTES    512                 /* includes the trailing \n */
#define NICK_MAX          16
#define OUT_MAX           (LINE_MAX_BYTES + NICK_MAX + 8)
#define SEND_TIMEOUT_MS   1000
#define LISTEN_BACKLOG    16
#define SHUTDOWN_WAIT_S   5
#define SESSION_STACK     (256 * 1024)
#define DEFAULT_PORT      5000
#define IP_STR_MAX        46                  /* INET6_ADDRSTRLEN, with NUL */

/* Compiler attributes (GCC/Clang). These are not constants, but they are
 * needed by several headers, and a separate header would add a ninth
 * module. */

/* Makes the compiler (with -Werror) reject a call that ignores the result. */
#define WARN_UNUSED       __attribute__((warn_unused_result))

/* Checks printf-style format strings: f = index of the format parameter,
 * a = index of the first variadic argument. */
#define PRINTF_FMT(f, a)  __attribute__((format(printf, f, a)))

#endif
