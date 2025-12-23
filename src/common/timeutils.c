#include <sys/time.h>
#include <math.h>
#include <stddef.h>

#include "timeutils.h"

const char time_format[] = "%08d.%03dms: ";

unsigned long get_time_in_us() {
    struct timeval now;
    (void) gettimeofday(&now, NULL);
    unsigned long time_us = now.tv_sec * 1000000 + now.tv_usec;
    return time_us;
}

void time_in_us_to_ms(unsigned long current_time_us, int* time_ms, int* time_us) {
    *time_ms = (int)(current_time_us / 1000);
    *time_us = (int)(current_time_us % 1000);
}

struct timespec get_wake_up_time(int time_ms) {
    struct timeval now;
    struct timespec relative_timeout, absolute_timeout;
    
    gettimeofday(&now, 0);
    relative_timeout.tv_sec = 0;
    relative_timeout.tv_nsec = time_ms * 1000000; // milliseconds -> nanoseconds
    absolute_timeout.tv_sec = now.tv_sec + relative_timeout.tv_sec;
    absolute_timeout.tv_nsec = 1000*now.tv_usec + relative_timeout.tv_nsec;
    while (absolute_timeout.tv_nsec >= 1000000000) {
        // deal with the carry
        absolute_timeout.tv_nsec -= 1000000000;
        absolute_timeout.tv_sec++;
    }
    return absolute_timeout;
}