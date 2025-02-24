#include <stdio.h>
#include <sys/resource.h>
#include <sys/time.h>
#include "adsb_time.h"       // Para getCurrentTime(), se disponível
#include "adsb_db.h"         // Para DB_open, DB_close, etc.
#include "adsb_createLog.h"  // Para LOG_add

void printCpuUsage(void) {
    struct rusage usage;
    getrusage(RUSAGE_SELF, &usage);
    printf("User CPU: %ld.%06ld s\n", usage.ru_utime.tv_sec, usage.ru_utime.tv_usec);
    printf("Sys  CPU: %ld.%06ld s\n", usage.ru_stime.tv_sec, usage.ru_stime.tv_usec);
    printf("Max RSS : %ld KB\n", usage.ru_maxrss);
}


/**
 * getCpuUsage - Coleta o uso de CPU e memória.
 * @user_cpu: ponteiro para armazenar tempo de CPU usuário (em segundos)
 * @sys_cpu: ponteiro para armazenar tempo de CPU sistema (em segundos)
 * @max_rss: ponteiro para armazenar memória máxima (ru_maxrss, em KB)
 */
void getCpuUsage(double *user_cpu, double *sys_cpu, long *max_rss) {
    struct rusage usage;
    if(getrusage(RUSAGE_SELF, &usage) == 0) {
        *user_cpu = usage.ru_utime.tv_sec + usage.ru_utime.tv_usec / 1000000.0;
        *sys_cpu  = usage.ru_stime.tv_sec + usage.ru_stime.tv_usec / 1000000.0;
        *max_rss  = usage.ru_maxrss;
    } else {
        *user_cpu = *sys_cpu = 0.0;
        *max_rss = 0;
    }
}