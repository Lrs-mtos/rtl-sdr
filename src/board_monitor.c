#include "board_monitor.h"
#include <stdio.h>
#include <sys/resource.h>
#include <sys/time.h>

void printCpuUsage(void) {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    printf("User CPU: %ld.%06ld s\n", usage.ru_utime.tv_sec, usage.ru_utime.tv_usec);
    printf("Sys  CPU: %ld.%06ld s\n", usage.ru_stime.tv_sec, usage.ru_stime.tv_usec);
    printf("Max RSS : %ld KB\n", usage.ru_maxrss);
}
