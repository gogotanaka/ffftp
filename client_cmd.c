#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <dirent.h>
#include "client_cmd.h"
#include "ftpdata.h"
#include "sock_util.h"

cmddef_t cmd_table[] = {
    { "quit", CMD_QUIT, TYP_QUIT, 0, 0, client_quit_fn },
    { "pwd", CMD_PWD, TYP_PWD, 0, 0, pwd_fn },
    { "cd", CMD_CD, TYP_CWD, 1, 0, cd_fn },
    { "dir", CMD_DIR, TYP_LIST, 0, 1, dir_fn },
    { "lpwd", CMD_LPWD, TYP_NONE, 0, 0, lpwd_fn },
    { "lcd", CMD_LCD, TYP_NONE, 1, 0, lcd_fn },
    { "ldir", CMD_LDIR, TYP_NONE, 0, 1, ldir_fn },
    { "get", CMD_GET, TYP_RETR, 1, 1, get_fn },
    { "put", CMD_PUT, TYP_STOR, 1, 1, put_fn }
};

int cmd_cnt = sizeof(cmd_table) / sizeof(cmddef_t);

int parse_args(cmddef_t cdef, char* buf, int* argc, char** args)
{
    *argc = 0;
#ifdef DEBUG
    printf("required argc: %d\n", cdef.required_argc);
#endif
    if (cdef.required_argc != 0) {
        int i;
        for (i = 0; i < cdef.required_argc; i++) {
            while (*buf == '\t' || *buf == ' ') {
                buf++;
            }
            if (*buf == '\0') {
                return -1;
            }
            char* tok;
            if ((tok = malloc(sizeof(char) * 256)) == NULL) {
                fprintf(stderr, "Failed to allocate memory\n");
                return -1;
            }
            char* tptr = tok;
            while (*buf != '\t' && *buf != ' ' && *buf != '\0') {
                *tptr++ = *buf++;
            }
#ifdef DEBUG
            printf("tok: %s\n", tok);
#endif
            args[(*argc)++] = tok;
        }
    }
#ifdef DEBUG
    printf("optional argc: %d\n", cdef.optional_argc);
#endif
    if (cdef.optional_argc != 0) {
        int i;
        for (i = 0; i < cdef.optional_argc; i++) {
            while (*buf == '\t' || *buf == ' ') {
                buf++;
            }
            if (*buf == '\0') {
                return 0;
            }
            char* tok;
            if ((tok = malloc(sizeof(char) * 256)) == NULL) {
                fprintf(stderr, "Failed to allocate memory\n");
                return -1;
            }
            char* tptr = tok;
            while (*buf != '\t' && *buf != ' ' && *buf != '\0') {
                *tptr++ = *buf++;
            }
#ifdef DEBUG
            printf("tok: %s\n", tok);
#endif
            args[(*argc)++] = tok;
        }
    }
    return 0;
}

cmdrun_t* parse_cmd(char* buf)
{
    while (*buf == '\t' || *buf == ' ') {
        buf++;
    }
    if (buf == '\0')
        return NULL;
    else {
        char* cmd;
        if ((cmd = malloc(sizeof(char) * 256)) == NULL) {
            return NULL;
        }
#ifdef DEBUG
        printf("rest buf: %s\n", buf);
#endif
        char* cptr = cmd;
        while (*buf != '\t' && *buf != ' ' && *buf != '\0') {
#ifdef DEBUG
            printf("buf head: %c\n", *buf);
#endif
            *cptr++ = *buf++;
        }
#ifdef DEBUG
        printf("cmd: %s\n", cmd);
#endif
        int i;
        cmddef_t matched_cmd;
        for (i = 0; i < cmd_cnt; i++) {
            cmddef_t cmd_data = cmd_table[i];
#ifdef DEBUG
            printf("compare name: '%s', cmd: '%s'\n", cmd_data.name, cmd);
#endif
            if (strcmp(cmd_data.name, cmd) == 0) {
                matched_cmd = cmd_data;
                int argc;
                char** args;
                args = malloc(sizeof(char*) * 256);
                if (parse_args(matched_cmd, buf, &argc, args) < 0) {
                    fprintf(stderr, "? Command syntax error\n");
                    return NULL;
                }
                else {
                    cmdrun_t* run;
                    if ((run = malloc(sizeof(cmdrun_t))) == NULL) {
                        fprintf(stderr, "Failed to allocate memory\n");
                        return NULL;
                    }
                    else {
                        run->type = matched_cmd.type;
                        cmdarg_t arg;
                        run->cmd = cmd;
                        arg.argc = argc;
                        arg.argv = args;
                        run->arg = arg;
                        run->handler_fn = matched_cmd.handler_fn;
                        return run;
                    }
                }
            }
        }
        fprintf(stderr, "? Unknown command\n");
        return NULL;
    }
}

/*
 * Handler function
 */

void handle_error_type(uint8_t type, uint8_t code)
{
    if (type == TYP_CMD_ERR) {
        switch (code) {
        case 0x01:
            fprintf(stderr, "? Command error: syntax error.\n");
            break;
        case 0x02:
            fprintf(stderr, "? Command error: unknown command.\n");
            break;
        case 0x03:
            fprintf(stderr, "? Command error: protocol error.\n");
            break;
        default:
            fprintf(stderr, "? Command error: Illigal error code.\n");
            break;
        }
    }
    else if (type == TYP_FILE_ERR) {
        switch (code) {
        case 0x00:
            fprintf(stderr, "? File error: Requested file does not exists.\n");
            break;
        case 0x01:
            fprintf(stderr, "? File error: Permission denied.\n");
            break;
        default:
            fprintf(stderr, "? File error: Illigal error code.\n");
            break;
            break;
        }
    }
    else if (type == TYP_UNKWN_ERR) {
        switch (code) {
        case 0x05:
            fprintf(stderr, "? Undefined error.\n");
            break;
        default:
            fprintf(stderr, "? Undefined error: unknown code.\n");
            break;
        }
    }
    else {
        fprintf(stderr, "? Internal error: Type %d is not error type.\n", type);
    }
}
void client_quit_fn(int sd, cmdarg_t data)
{
    //Sending Quit Command.
    myftph_t header;
    header.type = TYP_QUIT;
    header.code = 0x00;
    header.length = 0;
    if (send_pkt(sd, &header, sizeof header, 0) < 0) {
        perror("send_pkt");
    }
    //Waiting Response.
    ssize_t cnt;
    char* buf;
    if ((cnt = recv_pkt(sd, &header, &buf)) < 0) {
        exit(1);
    }
    else if (cnt == 0) {
        fprintf(stderr, "Socket closed.\n");
        exit(1);
    }
    else {
        if (header.type == TYP_OK && header.code == 0x00 && header.length == 0) {
            close(sd);
            exit(0);
        }
        else {
            switch (header.type) {
            case TYP_OK:
                fprintf(stderr, "Server response ok but illigal code or len.\n");
                close(sd);
                exit(1);
                break;
            case TYP_CMD_ERR:
            case TYP_UNKWN_ERR:
                handle_error_type(header.type, header.code);
                exit(1);
                break;
            default:
                fprintf(stderr, "? Illigal response type.\n");
                exit(1);
                break;
            }
        }
    }
}

void server_quit_fn(int sd, cmdarg_t data)
{
}

void pwd_fn(int sd, cmdarg_t arg)
{
    int cnt;
    myftph_t header;
    memset(&header, 0, sizeof(header));
    header.type = TYP_PWD;
    header.code = 0x00;
    header.length = 0;
    if (send_pkt(sd, &header, sizeof header, 0) < 0) {
        perror("send_pkt");
        exit(1);
    }
    char* buf;
    if ((cnt = recv_pkt(sd, &header, &buf)) < 0) {
        perror("recv_pkt");
        exit(1);
    }
    else if (cnt == 0) {
        fprintf(stderr, "Error: Server close socket unintentionally\n");
        exit(1);
    }
    else {
        char dirname[header.length + 1];
        strncpy(dirname, buf, header.length);
        dirname[header.length] = '\0';
        printf("%s\n", dirname);
    }
}

void cd_fn(int sd, cmdarg_t arg)
{
    myftph_t header;
    memset(&header, 0, sizeof(header));
    header.type = TYP_CWD;
    header.code = 0x00;
    header.length = strlen(arg.argv[0]);
    char pkt_data[sizeof(myftph_t) + header.length];
    memcpy(pkt_data, &header, sizeof(myftph_t));
    strncpy(pkt_data + sizeof(myftph_t), arg.argv[0], header.length);
    if (send_pkt(sd, pkt_data, sizeof pkt_data, 0) < 0) {
        perror("send_pkt");
    }
    char* buf;
    int cnt;
    if ((cnt = recv_pkt(sd, &header, &buf)) < 0) {
        exit(1);
    }
    else if (cnt == 0) {
        exit(1);
    }
    else {
        if (header.type != TYP_OK || header.code != 0x00 || header.length != 0) {
            handle_error_type(header.type, header.code);
        }
    }
}

void dir_fn(int sd, cmdarg_t arg)
{
    myftph_t header;
    //Sending packet.
    memset(&header, 0, sizeof(header));
    header.type = TYP_LIST;
    header.code = 0x00;
    if (arg.argc == 0) {
        header.length = 0;
        if (send_pkt(sd, &header, sizeof header, 0) < 0) {
            perror("send_pkt");
        }
    }
    else {
        char* dir = arg.argv[0];
        header.length = strlen(dir);
        char pkt_data[sizeof(myftph_t) + header.length];
        memcpy(pkt_data, &header, sizeof(myftph_t));
        strncpy(pkt_data + sizeof(myftph_t), dir, strlen(dir));
        if (send_pkt(sd, pkt_data, sizeof pkt_data, 0) < 0) {
            perror("send_pkt");
        }
    }
    //Recieve messages.
    int cnt;
    char* buf;
    if ((cnt = recv_pkt(sd, &header, &buf)) < 0) {
        perror("recv_pkt");
        exit(1);
    }
    else if (cnt == 0) {
        fprintf(stderr, "Error: Server close socket unintentionally\n");
        exit(1);
    }
    else {
        if (header.type == TYP_OK && header.code == 0x01 && header.length == 0) {
            //Starting retriving loop.
            while (1) {
                if ((cnt = recv_pkt(sd, &header, &buf)) < 0) {
                    perror("recv_pkt");
                }
                else if (cnt == 0) {
                    fprintf(stderr, "Error: Server close socket unintentionally\n");
                    exit(1);
                }
                else {
                    if (header.type == TYP_DATA) {
                        if (header.code == 0x00) {
                            //Last Message.
                            char name[header.length + 1];
                            memset(name, 0, sizeof(name));
                            strncpy(name, buf, header.length);
                            name[header.length] = '\0';
                            printf("%s\n", name);
                            break;
                        }
                        else if (header.code == 0x01) {
                            //Data Messge continue.
                            char name[header.length + 1];
                            memset(name, 0, sizeof(name));
                            strncpy(name, buf, header.length);
                            name[header.length] = '\0';
                            printf("%s\n", name);
                        }
                    }
                    else {
                        fprintf(stderr, "Illigal message type\n");
                        break;
                    }
                }
            }
        }
        else {
            handle_error_type(header.type, header.code);
        }
    }
}

void lpwd_fn(int sd, cmdarg_t arg)
{
    char pathname[512];
    getcwd(pathname, sizeof pathname);
    printf("%s\n", pathname);
}

void lcd_fn(int sd, cmdarg_t arg)
{
    chdir(arg.argv[0]);
}

void ldir_fn(int sd, cmdarg_t arg)
{
    /*
  pid_t pid = fork();
  if(pid < 0) {
    fprintf(stderr, "LDIR error\n");
  } else if(pid == 0) {
    //Child
    int i;
    char *args[arg.argc + 1];
    args[0] = "ls";
    for(i = 1; i < arg.argc; i++) {
      args[i] = arg.argv[i - 1];
    }
    args[i] = NULL;
    execvp(args[0], args);
  } else {
    //Parent.
  }
  */
    //Get directiroy list.
    char dirname[512];
    getcwd(dirname, sizeof dirname);
    printf("%s\n", dirname);
    DIR* dir;
#ifdef DEBUG
    printf("dirname: '%s'\n", dirname);
#endif
    if ((dir = opendir(dirname)) == NULL) {
        perror("opendir");
    }
    else {
        struct dirent* dp;
        for (dp = readdir(dir); dp != NULL; dp = readdir(dir)) {
            printf("%s\n", dp->d_name);
        }
        closedir(dir);
    }
}

void get_fn(int sd, cmdarg_t arg)
{
    /*
   * Retrive file from server.
   * 
   * Opening destrination to check if destination file is writeable or not.
   * But using temporary file to store data to avoid race-condition ..
   * (Imagine client connecting to own server and put (or get) on same file.)
   */
    myftph_t header;
    char* src_fname;
    char* dst_fname;
    char* buf;
    int res;
    //Open destination file.
    src_fname = arg.argv[0];
    if (arg.argc == 1) {
        dst_fname = src_fname;
    }
    else {
        dst_fname = arg.argv[1];
    }
    FILE* dst_fp;
    if ((dst_fp = fopen(dst_fname, "wb")) == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", dst_fname);
        return;
    }
    //Send get message.
    memset(&header, 0, sizeof(header));
    header.type = TYP_RETR;
    header.code = 0x00;
    header.length = strlen(src_fname);
    char pkt_data[sizeof(myftph_t) + header.length];
    memcpy(pkt_data, &header, sizeof(myftph_t));
    strncpy(pkt_data + sizeof(myftph_t), src_fname, header.length);
    if (send_pkt(sd, pkt_data, sizeof pkt_data, 0) < 0) {
        perror("send_pkt");
    }
    //Wait ok response.
    if ((res = recv_pkt(sd, &header, &buf)) < 0) {
        perror("recv_pkt");
        exit(1);
    }
    else if (res == 0) {
        fprintf(stderr, "Server close socket.\n");
        exit(1);
    }
    else {
        if (header.type == TYP_OK && header.code == 0x01 && header.length == 0) {
            //Start Retriving file.
            while (1) {
                if ((res = recv_pkt(sd, &header, &buf)) < 0) {
                    perror("recv_pkt");
                    exit(1);
                }
                else if (res == 0) {
                    fprintf(stderr, "Server close socket.\n");
                    exit(1);
                }
                else {
                    if (fwrite(buf, sizeof(char), header.length, dst_fp) < header.length) {
                        fprintf(stderr, "Failed to write file\n");
                        exit(1);
                    }
                    if (header.code == 0x00) {
#ifdef DEBUG
                        fprintf(stderr, "File end\n");
#endif
                        break;
                    }
                }
            }
        }
        else {
            if (header.length != 0) {
                free(buf);
            }
            handle_error_type(header.type, header.code);
        }
    }
}

void put_fn(int sd, cmdarg_t arg)
{
    //seteup src and dst file-name.
    char* src_fname = arg.argv[0];
    char* dst_fname;
    if (arg.argc == 1) {
        dst_fname = arg.argv[0];
    }
    else {
        dst_fname = arg.argv[1];
    }
    //Opening src file.
    FILE* src_fp;
    if ((src_fp = fopen(src_fname, "rb")) == NULL) {
        fprintf(stderr, "Failed to open file: %s\n", src_fname);
    }
    else {
        //Sending put msg.
        myftph_t header;
        memset(&header, 0, sizeof(header));
        header.type = TYP_STOR;
        header.code = 0x00;
        header.length = strlen(dst_fname);
        char pkt_data[sizeof(myftph_t) + header.length];
        memcpy(pkt_data, &header, sizeof(myftph_t));
        strncpy(pkt_data + sizeof(myftph_t), dst_fname, header.length);
        if (send_pkt(sd, pkt_data, sizeof pkt_data, 0) < 0) {
            perror("send_pkt");
        }
        //Wait reply message.
        char* buf;
        int res;
        if ((res = recv_pkt(sd, &header, &buf)) < 0) {
            perror("recv_pkt");
            exit(1);
        }
        if (res == 0) {
            fprintf(stderr, "Server close socket\n");
            exit(1);
        }
        else {
            if (header.type == TYP_OK && header.code == 0x02 && header.length == 0) {
                //Sending data message.
                while (1) {
                    size_t cnt;
                    char data[DATA_LEN];
                    if ((cnt = fread((void*)data, sizeof(char), DATA_LEN, src_fp)) < DATA_LEN) {
                        fprintf(stderr, "cnt: %d\n", (int)cnt);
                        //Check if error occured or just end of file.
                        if (feof(src_fp) != 0) {
#ifdef DEBUG
                            fprintf(stderr, "FEOF sending last packet\n");
#endif
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
                            fclose(src_fp);
                        }
                        else {
                            //error..
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
            else {
                handle_error_type(header.type, header.code);
            }
        }
    }
}
