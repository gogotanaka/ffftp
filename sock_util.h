#ifndef __SOCK_UTIL_H__
#define __SOCK_UTIL_H__

#include <sys/socket.h>
#include "ftpdata.h"

ssize_t send_pkt(int socket, const void* buffer, size_t length, int flags);

int recv_pkt(int socket, myftph_t*, char**);

#endif
