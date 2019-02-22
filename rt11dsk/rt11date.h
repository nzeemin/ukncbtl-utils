#ifndef _RT11DATE_H
#define _RT11DATE_H
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <time.h>
#include <string.h>

// https://stackoverflow.com/questions/1537964/visual-c-equivalent-of-gccs-attribute-packed
#if defined(__GNUC__)
    #define PACK( __Declaration__ ) __Declaration__ __attribute__((__packed__))
#else
    #define PACK( __Declaration__ ) __pragma( pack(push, 1) ) __Declaration__ __pragma( pack(pop) )
#endif

union rt11date {
    uint16_t    pac;
PACK(
    struct __attribute__((__packed__))  __i {
        uint8_t year:5;
        uint8_t day:5;
        uint8_t mon:4;
        uint8_t age:2;
    } i); 
    
};

uint16_t clock2rt11date(const time_t clock);
void rt11date_str(uint16_t date, char* buf, size_t sz);

#ifdef __cplusplus
}
#endif
#endif
