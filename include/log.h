#ifndef TCPCHAT_LOG_H
#define TCPCHAT_LOG_H

void log_msg(int level, const char *fmt, ...);
void log_errno(const char *what);

#endif
