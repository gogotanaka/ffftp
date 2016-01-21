#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <signal.h>
#include <unistd.h>
#include <netdb.h>
#include "ftpdata.h"
#include "client_cmd.h"
#include "server_cmd.h"
#include "sock_util.h"

void usage();
int ftp_child_main(int sd);

//Signal handler
int parent_quit_flag = 0;
int child_quit_flag = 0;
void parent_trap(int);
void child_trap(int);

int mainsd;

typedef struct child {
    int sd;
    pid_t pid;
    struct child* next;
    struct child* prev;
} child_t;

child_t* child_root;

int main(int argc, char** argv)
{
    if (argc != 2) {
        usage();
        exit(1);
    }
    else {
        char* dir = argv[1];
        if (chdir(dir) < 0) {
            perror("chdir");
            exit(1);
        }
    }

    int childsd;
    pid_t childpid;
    struct addrinfo hints, *res;
    struct sockaddr_storage sin;
    char* srv;
    int err, sktlen;
    /*
   * setup signal handler
   */
    //SIGINT handler.
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = parent_trap;
    sa.sa_flags |= SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) != 0) {
        fprintf(stderr, "Failed to attach sigaction\n");
        exit(EXIT_FAILURE);
    }

    srv = "50021";
    memset(&hints, 0, sizeof hints);
    hints.ai_flags = AI_PASSIVE;
    hints.ai_socktype = SOCK_STREAM;
    if ((err = getaddrinfo(NULL, srv, &hints, &res)) < 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
        exit(1);
    }

    if ((mainsd = socket(res->ai_family, res->ai_socktype, res->ai_protocol)) < 0) {
        perror("socket");
        exit(1);
    }

    int on = 1;
    setsockopt(mainsd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    if (bind(mainsd, res->ai_addr, res->ai_addrlen) < 0) {
        perror("bind");
        exit(1);
    }
    freeaddrinfo(res);

    child_root = malloc(sizeof(child_t));
    child_root->next = child_root->prev = child_root;

    while (!parent_quit_flag) {
        if (listen(mainsd, 5) < 0) {
            perror("listen");
            exit(1);
        }
        sktlen = sizeof(struct sockaddr_storage);
        if ((childsd = accept(mainsd, (struct sockaddr*)&sin, (socklen_t*)&sktlen)) < 0) {
            perror("accept");
            exit(1);
        }
        if ((childpid = fork()) < 0) {
            perror("fork");
            exit(1);
        }
        else if (childpid == 0) {
            //child process.
            child_t* new_child = malloc(sizeof(child_t));
            new_child->pid = getpid();
            new_child->sd = childsd;
            new_child->prev = child_root;
            new_child->next = child_root->next;
            child_root->next->prev = new_child;
            child_root->next = new_child;
            int res = ftp_child_main(childsd);
#ifdef DEBUG
            printf("Child quit\n");
#endif
            exit(res);
        }
        else {
            //Parent process.
        }
    }
    return 0;
}

void usage()
{
    fprintf(stderr, "./myftpd <working-directory\n");
}

int ftp_child_main(int sd)
{
    int cnt;
    myftph_t header;
    char* buf;

    /*
   * setup signal handler
   */
    //SIGINT handler.
    struct sigaction sa;
    memset(&sa, 0, sizeof(struct sigaction));
    sa.sa_handler = child_trap;
    sa.sa_flags |= SA_RESTART;

    if (sigaction(SIGINT, &sa, NULL) != 0) {
        fprintf(stderr, "Failed to attach sigaction\n");
        exit(EXIT_FAILURE);
    }
    while (1) {
        if ((cnt = recv_pkt(sd, &header, &buf)) < 0) {
            perror("recv_pkt");
            return 1;
        }
        else if (cnt == 0) {
            //fprintf(stderr,"Child client closed connection\n");
            return 0;
        }
        else {
            switch (header.type) {
            case TYP_QUIT:
                server_quit_fn(sd, &header, NULL);
                break;
            case TYP_PWD:
                server_pwd_fn(sd, &header, NULL);
                break;
            case TYP_CWD:
                server_cd_fn(sd, &header, buf);
                break;
            case TYP_LIST:
                server_dir_fn(sd, &header, buf);
                break;
            case TYP_RETR:
                server_get_fn(sd, &header, buf);
                break;
            case TYP_STOR:
                server_put_fn(sd, &header, buf);
                break;
            default: {
                myftph_t header;
                header.type = TYP_CMD_ERR;
                header.code = 0x02;
                header.length = 0;
                if (send_pkt(sd, &header, sizeof header, 0) < 0) {
                    perror("send_pkt");
                }
            } break;
            }
            if (header.length != 0) {
                free(buf);
            }
        }
    }
    close(sd);
    return 0;
}

void parent_trap(int sig)
{
#ifdef DEBUG
    fprintf(stderr, "Parent trap\n");
#endif
    close(mainsd);
    exit(0);
}

void child_trap(int sig)
{
#ifdef DEBUG
    fprintf(stderr, "Child trap\n");
#endif
    pid_t child_pid = getpid();
    child_t* crr;
    for (crr = child_root->next; crr != child_root; crr = crr->next) {
        if (crr->pid == child_pid) {
            close(crr->sd);
            exit(0);
        }
    }
    exit(1);
}
