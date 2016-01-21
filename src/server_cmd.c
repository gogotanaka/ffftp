#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include "ftpdata.h"
#include "client_cmd.h"
#include "server_cmd.h"
#include "sock_util.h"

typedef struct dir_list {
    char dirname[1024];
    struct dir_list* next;
} dir_list_t;

void server_quit_fn(int sd, myftph_t* h, char* arg)
{
    myftph_t header;
    if (h->length == 0) {
        header.type = TYP_OK;
        header.code = 0x00;
        header.length = 0;
    }
    else {
        header.type = TYP_CMD_ERR;
        header.code = 0x01;
        header.length = 0;
    }
    if (send_pkt(sd, &header, sizeof header, 0) < 0) {
        perror("send_pkt");
    }
}

void server_pwd_fn(int sd, myftph_t* h, char* arg)
{
    myftph_t header;
    if (h->length != 0) {
        header.type = TYP_CMD_ERR;
        header.code = 0x01;
        header.length = 0;
        if (send_pkt(sd, &header, sizeof header, 0) < 0) {
            perror("send_pkt");
        }
    }
    else {
        char dirname[DATA_LEN];
        if (getcwd(dirname, sizeof dirname) == NULL) {
            fprintf(stderr, "Failed to get current workind directory\n");
            header.type = TYP_UNKWN_ERR;
            header.code = 0x05;
            header.length = 0;
            if (send_pkt(sd, &header, sizeof header, 0) < 0) {
                perror("send_pkt");
            }
        }
        else {
            header.type = TYP_OK;
            header.code = 0x00;
            header.length = strlen(dirname);
            char pkt_data[sizeof(myftph_t) + header.length];
            memcpy(pkt_data, &header, sizeof(myftph_t));
            strncpy(pkt_data + sizeof(myftph_t), dirname, header.length);
            if (send_pkt(sd, &pkt_data, sizeof pkt_data, 0) < 0) {
                perror("send_pkt");
            }
        }
    }
}

void server_cd_fn(int sd, myftph_t* header, char* buf)
{
    if (header->length == 0) {
        myftph_t h;
        h.type = TYP_CMD_ERR;
        h.code = 0x01;
        h.length = 0;
        if (send_pkt(sd, &h, sizeof h, 0) < 0) {
            perror("send_pkt");
        }
    }
    else {
        char dirname[header->length + 1];
        strncpy(dirname, buf, header->length);
        dirname[header->length] = '\0';
#ifdef DEBUG
        fprintf(stderr, "change cwd: '%s'\n", dirname);
#endif
        errno = 0;
        if (chdir(dirname) < 0) {
            fprintf(stderr, "Failed to change working directory\n");
            switch (errno) {
            case ENOENT: {
                myftph_t h;
                h.type = TYP_FILE_ERR;
                h.code = 0x00;
                h.length = 0;
                if (send_pkt(sd, &h, sizeof h, 0) < 0) {
                    perror("send_pkt");
                }
            } break;
            case EACCES: {
                myftph_t h;
                h.type = TYP_FILE_ERR;
                h.code = 0x01;
                h.length = 0;
                if (send_pkt(sd, &h, sizeof h, 0) < 0) {
                    perror("send_pkt");
                }
            } break;
            default: {
                myftph_t h;
                h.type = TYP_UNKWN_ERR;
                h.code = 0x05;
                h.length = 0;
                if (send_pkt(sd, &h, sizeof h, 0) < 0) {
                    perror("send_pkt");
                }
            } break;
            }
        }
        else {
            myftph_t new_header;
            new_header.type = TYP_OK;
            new_header.code = 0x00;
            new_header.length = 0;
            if (send_pkt(sd, &new_header, sizeof new_header, 0) < 0) {
                perror("send_pkt");
            }
        }
    }
}

void server_dir_fn(int sd, myftph_t* header, char* buf)
{
    //ensure dirname.
    char dirname[DATA_LEN];
    memset(dirname, 0, sizeof(dirname));
    if (header->length == 0) {
#ifdef DEBUG
        fprintf(stderr, "get current working directory\n");
#endif
        if (getcwd(dirname, sizeof dirname) == NULL) {
            fprintf(stderr, "Failed to get current working directory.\n");
            //Return error.
            myftph_t header;
            header.type = TYP_UNKWN_ERR;
            header.code = 0x05;
            header.length = 0;
            if (send_pkt(sd, &header, sizeof header, 0) < 0) {
                perror("send_pkt");
            }
        }
    }
    else {
        strncpy(dirname, buf, header->length);
    }

    //Get directiroy list.
    DIR* dir;
#ifdef DEBUG
    printf("dirname: '%s'\n", dirname);
#endif
    if ((dir = opendir(dirname)) == NULL) {
        perror("opendir");
    }
    else {
        dir_list_t *root, *crr;
        if ((root = malloc(sizeof(dir_list_t))) == NULL) {
            fprintf(stderr, "Failed to allocate dir_list_t\n");
        }
        struct dirent* dp;
        for (dp = readdir(dir), crr = root; dp != NULL; dp = readdir(dir)) {
            dir_list_t* d;
            if ((d = malloc(sizeof(dir_list_t))) == NULL) {
                fprintf(stderr, "Failed to allocate dir_list_t\n");
                break;
            }
            else {
                strncpy(d->dirname, dp->d_name, sizeof(d->dirname));
                crr->next = d;
                crr = d;
            }
        }
        crr->next = NULL;
        closedir(dir);

        //send_pkt all dirta;
        myftph_t header;
        header.type = TYP_OK;
        header.code = 0x01;
        header.length = 0;
        if (send_pkt(sd, &header, sizeof header, 0) < 0) {
            perror("send_pkt");
        }
        for (crr = root->next; crr->next != NULL; crr = crr->next) {
            header.type = TYP_DATA;
            header.code = 0x01;
            header.length = strlen(crr->dirname);
            char pkt_data[sizeof(myftph_t) + header.length];
            memcpy(pkt_data, &header, sizeof(myftph_t));
            strncpy(pkt_data + sizeof(myftph_t), crr->dirname, header.length);
            if (send_pkt(sd, &pkt_data, sizeof pkt_data, 0) < 0) {
                perror("send_pkt");
            }
        }
        header.type = TYP_DATA;
        header.code = 0x00;
        header.length = strlen(crr->dirname);
        char pkt_data[sizeof(myftph_t) + header.length];
        memcpy(pkt_data, &header, sizeof(myftph_t));
        strncpy(pkt_data + sizeof(myftph_t), crr->dirname, header.length);
        if (send_pkt(sd, &pkt_data, sizeof pkt_data, 0) < 0) {
            perror("send_pkt");
        }
    }
}

void server_get_fn(int sd, myftph_t* h, char* arg)
{
    if (h->length == 0) {
        myftph_t header;
        header.type = TYP_CMD_ERR;
        header.code = 0x01;
        header.length = 0;
        if (send_pkt(sd, &header, sizeof header, 0) < 0) {
            perror("send_pkt");
        }
    }
    else {
        //Setup src filename.
        char src_fname[h->length + 1];
        strncpy(src_fname, arg, h->length);
        src_fname[h->length] = '\0';
        //Trying to open src file.
        FILE* src_fp;
        if ((src_fp = fopen(src_fname, "rb")) == NULL) {
            fprintf(stderr, "Failed to open file: %s\n", src_fname);
            switch (errno) {
            case ENOENT: {
                myftph_t header;
                header.type = TYP_FILE_ERR;
                header.code = 0x00;
                header.length = 0;
                if (send_pkt(sd, &header, sizeof header, 0) < 0) {
                    perror("send_pkt");
                    exit(1);
                }
            } break;
            case EACCES: {
                myftph_t header;
                header.type = TYP_FILE_ERR;
                header.code = 0x01;
                header.length = 0;
                if (send_pkt(sd, &header, sizeof header, 0) < 0) {
                    perror("send_pkt");
                    exit(1);
                }
            } break;
            default: {
                myftph_t header;
                header.type = TYP_UNKWN_ERR;
                header.code = 0x05;
                header.length = 0;
                if (send_pkt(sd, &header, sizeof header, 0) < 0) {
                    perror("send_pkt");
                    exit(1);
                }
            } break;
            }
        }
        else {
            //Retun OK message.
            myftph_t header;
            header.type = TYP_OK;
            header.code = 0x01;
            header.length = 0;
            if (send_pkt(sd, &header, sizeof header, 0) < 0) {
                perror("send_pkt");
                exit(1);
            }
            //Start sending file contents.
            while (1) {
                size_t cnt;
                char data[DATA_LEN];
                if ((cnt = fread((void*)data, sizeof(char), DATA_LEN, src_fp)) < DATA_LEN) {
                    fprintf(stderr, "cnt: %d\n", (int)cnt);
                    //Check if error occured or just end of file.
                    if (feof(src_fp)) {
                        fclose(src_fp);
                        //sending data as data packet.
                        header.type = TYP_DATA;
                        header.code = 0x00;
                        header.length = cnt;
                        char pkt_data[sizeof(myftph_t) + header.length];
                        memcpy(pkt_data, &header, sizeof(myftph_t));
                        memcpy(pkt_data + sizeof(myftph_t), data, header.length);
                        if (send_pkt(sd, pkt_data, sizeof pkt_data, 0) < 0) {
                            perror("send_pkt");
                            exit(1);
                        }
                        break;
                    }
                    else {
                        header.type = TYP_UNKWN_ERR;
                        header.code = 0x05;
                        header.length = 0;
                        if (send_pkt(sd, &header, sizeof header, 0) < 0) {
                            perror("send_pkt");
                            exit(1);
                        }
                        fclose(src_fp);
                        break;
                    }
                }
                else {
#ifdef DEBUG
                    fprintf(stderr, "Send first data packet\n");
#endif
                    //sending data as data packet.
                    header.type = TYP_DATA;
                    header.code = 0x01;
                    header.length = DATA_LEN;
                    char pkt_data[sizeof(myftph_t) + header.length];
                    memcpy(pkt_data, &header, sizeof(myftph_t));
                    memcpy(pkt_data + sizeof(myftph_t), data, header.length);
                    if (send_pkt(sd, pkt_data, sizeof pkt_data, 0) < 0) {
                        perror("send_pkt");
                        exit(1);
                    }
                }
            }
        }
    }
}

void server_put_fn(int sd, myftph_t* h, char* arg)
{
    if (h->length == 0) {
        myftph_t header;
        header.type = TYP_CMD_ERR;
        header.code = 0x01;
        header.length = 0;
        if (send_pkt(sd, &header, sizeof header, 0) < 0) {
            perror("send_pkt");
        }
    }
    else {
        //Get dst filename.
        char fname[h->length + 1];
        strncpy(fname, arg, h->length);
        fname[h->length] = '\0';
        //Check if dst file writeable or not.
        FILE* fp;
        errno = 0;
        if ((fp = fopen(fname, "wb")) == NULL) {
            fprintf(stderr, "Failed to open file\n");
            switch (errno) {
            case ENOENT: {
                myftph_t header;
                header.type = TYP_FILE_ERR;
                header.code = 0x00;
                header.length = 0;
                if (send_pkt(sd, &header, sizeof header, 0) < 0) {
                    perror("send_pkt");
                    exit(1);
                }
            } break;
            case EACCES: {
                myftph_t header;
                header.type = TYP_FILE_ERR;
                header.code = 0x01;
                header.length = 0;
                if (send_pkt(sd, &header, sizeof header, 0) < 0) {
                    perror("send_pkt");
                    exit(1);
                }
            } break;
            default: {
                myftph_t header;
                header.type = TYP_UNKWN_ERR;
                header.code = 0x05;
                header.length = 0;
                if (send_pkt(sd, &header, sizeof header, 0) < 0) {
                    perror("send_pkt");
                    exit(1);
                }
            } break;
            }
        }
        else {
            //Retun OK message.
            myftph_t header;
            header.type = TYP_OK;
            header.code = 0x02;
            header.length = 0;
            if (send_pkt(sd, &header, sizeof header, 0) < 0) {
                perror("send_pkt");
                exit(1);
            }
            //Start recieving file data.
            while (1) {
                char* buf;
                int res;
                if ((res = recv_pkt(sd, &header, &buf)) < 0) {
                    perror("recv_pkt");
                    exit(1);
                }
                else if (res == 0) {
                    fprintf(stderr, "Client close socket.\n");
                    exit(1);
                }
                else {
                    if (header.type == TYP_DATA) {
                        if (fwrite(buf, sizeof(char), header.length, fp) < header.length) {
                            fprintf(stderr, "Failed to write file: %s\n", fname);
                            exit(1);
                        }
                        if (header.code == 0x00) {
                            fclose(fp);
                            break;
                        }
                    }
                    else {
                        header.type = TYP_CMD_ERR;
                        header.code = 0x03;
                        header.length = 0;
                        if (send_pkt(sd, &header, sizeof header, 0) < 0) {
                            perror("send_pkt");
                            exit(1);
                        }
                    }
                }
            }
        }
    }
}
