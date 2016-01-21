#ifndef __CLIENT_CMD_H__
#define __CLIENT_CMD_H__

//Global command id.
#define CMD_QUIT 0
#define CMD_PWD 1
#define CMD_CD 2
#define CMD_DIR 3
#define CMD_LPWD 4
#define CMD_LCD 5
#define CMD_LDIR 6
#define CMD_GET 7
#define CMD_PUT 8

//Command types
#define TYP_NONE 0x00

#define TYP_QUIT 0x01
#define TYP_PWD 0x02
#define TYP_CWD 0x03
#define TYP_LIST 0x04
#define TYP_RETR 0x05
#define TYP_STOR 0x06

#define TYP_OK 0x10
#define TYP_CMD_ERR 0x11
#define TYP_FILE_ERR 0x12
#define TYP_UNKWN_ERR 0x13
#define TYP_DATA 0x20

typedef struct cmdarg {
    int argc;
    char** argv;
} cmdarg_t;

typedef struct cmddef {
    char* name;
    int code;
    int type;
    int required_argc;
    int optional_argc;
    void (*handler_fn)(int sd, cmdarg_t data);
} cmddef_t;

typedef struct cmdrun {
    char* cmd;
    int type;
    cmdarg_t arg;
    void (*handler_fn)(int sd, cmdarg_t data);
} cmdrun_t;

extern cmddef_t cmd_table[];

extern int cmd_cnt;

cmdrun_t* parse_cmd(char* buf);

void client_quit_fn(int, cmdarg_t);
void pwd_fn(int, cmdarg_t);
void cd_fn(int, cmdarg_t);
void dir_fn(int, cmdarg_t);
void lpwd_fn(int, cmdarg_t);
void lcd_fn(int, cmdarg_t);
void ldir_fn(int, cmdarg_t);
void get_fn(int, cmdarg_t);
void put_fn(int, cmdarg_t);

#endif
