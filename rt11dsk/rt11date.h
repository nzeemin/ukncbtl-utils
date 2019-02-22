#ifndef _RT11DATE_H
#define _RT11DATE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <time.h>
#include <string.h>

union rt11date {
    uint16_t    pac;
    struct __attribute__((__packed__))  __i {
        uint8_t year:5;
        uint8_t day:5;
        uint8_t mon:4;
        uint8_t age:2;
    } i; 
};

uint16_t clock2rt11date(const time_t clock);
void rt11date_str(uint16_t date, char* buf, size_t sz);

#ifdef __cplusplus
}
#endif
#endif
