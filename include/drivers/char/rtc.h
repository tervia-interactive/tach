#ifndef DRIVERS_CHAR_RTC_H
#define DRIVERS_CHAR_RTC_H

#include <kernel/types.h>

typedef struct {
    uint8_t second;
    uint8_t minute;
    uint8_t hour;
    uint8_t day;
    uint8_t month;
    uint16_t year;
} rtc_time_t;

int rtc_init(void);
int rtc_read_time(rtc_time_t *time);
int rtc_set_time(const rtc_time_t *time);

#endif /* DRIVERS_CHAR_RTC_H */
