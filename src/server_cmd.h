#ifndef __SERVER_CMD_H__
#define __SERVER_CMD_H__

#include "ftpdata.h"

void server_quit_fn(int, myftph_t*, char*);
void server_pwd_fn(int, myftph_t*, char*);
void server_cd_fn(int, myftph_t*, char*);
void server_dir_fn(int, myftph_t*, char*);
void server_get_fn(int, myftph_t*, char*);
void server_put_fn(int, myftph_t*, char*);

#endif
