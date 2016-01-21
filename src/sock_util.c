#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include "ftpdata.h"
#include "sock_util.h"

void show_pkt_info(char* pkt_data);

ssize_t send_pkt(int socket, const void* buffer, size_t length, int flags)
{
    ssize_t cnt;
    if ((cnt = send(socket, buffer, length, flags)) < 0) {
        perror("send");
    }
    else if (cnt == 0) {
        fprintf(stderr, "Socket closed.\n");
    }
    else {
#ifdef DEBUG
        fprintf(stderr, "Send pkt\n");
        show_pkt_info((char*)buffer);
#endif
    }
    return cnt;
}

int recv_pkt(int socket, myftph_t* header, char** data)
{
    ssize_t cnt;
    if ((cnt = recv(socket, header, sizeof(myftph_t), 0)) < 0) {
        perror("recv");
        return -1;
    }
    else if (cnt == 0) {
        return 0;
    }
    else {
#ifdef DEBUG
        fprintf(stderr, "Recv pkt\n");
        show_pkt_info((char*)header);
#endif
        if (header->length != 0) {
            if ((*data = malloc(sizeof(char) * header->length)) == NULL) {
                fprintf(stderr, "Failed to allocate data buffer\n");
                return -1;
            }
            if ((cnt = recv(socket, *data, header->length, 0)) < 0) {
                perror("recv");
                return -1;
            }
            else {
                return cnt;
            }
        }
        else {
            return cnt;
        }
    }
}

void show_pkt_info(char* pkt_data)
{
    myftph_t* header = (myftph_t*)pkt_data;
    fprintf(stderr, "==== PKT INFO ====\n");
    fprintf(stderr, "\ttype: 0x%x\n", header->type);
    fprintf(stderr, "\tcode: 0x%x\n", header->code);
    fprintf(stderr, "\tlength: 0x%x\n", header->length);
    /*
     if(header->length != 0) {
     char buff[header->length + 1];
     strncpy(buff, data, header->length);
     buff[header->length] = '\0';
     printf("\tdata: %s\n", buff);
     }
     */
}
