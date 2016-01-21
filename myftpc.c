#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>

#include "client_cmd.h"

int client_main(int);
void child_trap(int);

int sd = -1;

int main(int argc, char** argv)
{
    struct addrinfo hints, *res;
    char *host, *serv;
    int err;

    host = argv[1];
    serv = "50021";

    memset(&hints, 0, sizeof hints);
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_family = AF_INET;

    if ((err = getaddrinfo(host, serv, &hints, &res)) < 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        exit(1);
    }

    if ((sd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
        perror("socket");
        exit(1);
    }

    if (connect(sd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("connect");
        exit(1);
    }

    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = child_trap;
    sa.sa_flags |= SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) != 0) {
        fprintf(stderr, "Failed to attach sigaction\n");
        exit(EXIT_FAILURE);
    }

    int client_res = client_main(sd);
    close(sd);
    freeaddrinfo(res);
    return client_res;
}

int client_main(int sd)
{
    char buf[256];
    char cmd[256];
    char** args;

    args = malloc(sizeof(char*) * 256);

    while (1) {
        memset(buf, 0, sizeof buf);
        memset(cmd, 0, sizeof cmd);
        printf("myFTP%%");
        if (fgets(buf, sizeof buf, stdin) == NULL) {
            putchar('\n');
            return 0;
        }
        char* p;
        if ((p = strchr(buf, '\n')) != NULL) {
            *p = '\0';
        }

        cmdrun_t* res;
        if ((res = parse_cmd(buf)) == NULL) {
            fprintf(stderr, "parse error\n");
            continue;
        }

#ifdef DEBUG
        int i;
        for (i = 0; i < res->arg.argc; i++) {
            printf("arg(%d) = %s\n", i, res->arg.argv[i]);
        }
#endif

        res->handler_fn(sd, res->arg);
    }
}

void child_trap(int sig)
{
    printf("Clid trap\n");
    if (sd != -1)
        close(sd);
    exit(1);
}
