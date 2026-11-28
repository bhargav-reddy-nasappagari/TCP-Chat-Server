#ifndef TCPCHAT_LOG_H
#define TCPCHAT_LOG_H

#include "config.h"   /* PRINTF_FMT */

/* Severity levels. Named LOGL_* because <syslog.h> defines LOG_ERR and
 * LOG_INFO as macros, which would clash if both headers were included. */
enum {
    LOGL_INFO,
    LOGL_WARN,
    LOGL_ERR
};

/* Writes one timestamped line to stderr: "<time> <LEVEL> <message>\n".
 * The whole line is formatted into a local buffer and emitted with a single
 * fprintf, so lines from different threads never interleave.
 * Thread-safe. Failures are ignored by design: logging must never take the
 * server down, so there is no return value. */
void log_msg(int level, const char *fmt, ...) PRINTF_FMT(2, 3);

/* Logs "<what>: <strerror text of errno>" at LOGL_ERR, using strerror_r
 * (not strerror, which is not thread-safe). Call it immediately after the
 * failing call, before anything else can change errno. Thread-safe. */
void log_errno(const char *what);

#endif
