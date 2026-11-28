#ifndef TCPCHAT_SERVER_H
#define TCPCHAT_SERVER_H

#include <signal.h>
#include <stdint.h>

/* The program's only global: set by the SIGINT/SIGTERM handler, polled by
 * the accept loop. Defined in server.c. */
extern volatile sig_atomic_t g_stop;

/* Runs the server on port until g_stop is set, then shuts down gracefully.
 * Returns the process exit code: 0 on a clean shutdown, 1 if the listening
 * socket could not be created. */
int server_run(uint16_t port);

#endif
