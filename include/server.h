#ifndef TCPCHAT_SERVER_H
#define TCPCHAT_SERVER_H

#include <stdint.h>
#include <signal.h> 

extern volatile sig_atomic_t g_stop;
int server_run(uint16_t port);

#endif
