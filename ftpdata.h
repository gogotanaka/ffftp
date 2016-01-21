#ifndef __FTPDATA_H__
#define __FTPDATA_H__

#include <stdint.h>

#define DATA_LEN 1024

typedef struct myftph {
    uint8_t type;
    uint8_t code;
    uint16_t length;
} myftph_t;

#endif
