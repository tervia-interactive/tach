/* tach Operating System - RTC Header */
#ifndef _DRIVERS_RTC_H
#define _DRIVERS_RTC_H

#include <kernel/types.h>
#include <stdint.h>

/* RTC date/time structure */
typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} rtc_time_t;

/* Initialize RTC driver */
int rtc_init(void);

/* Get current time from RTC */
int rtc_get_time(rtc_time_t *time);

#endif /* _DRIVERS_RTC_H */
