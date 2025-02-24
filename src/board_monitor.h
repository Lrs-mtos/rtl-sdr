#ifndef MONITOR_H
#define MONITOR_H

void printCpuUsage(void);
void getCpuUsage(double *user_cpu, double *sys_cpu, long *max_rss);

#endif
