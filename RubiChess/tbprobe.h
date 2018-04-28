#ifndef TBPROBE_H
#define TBPROBE_H

extern int TBlargest; // 5 if 5-piece tables, 6 if 6-piece tables were found.

void init_tablebases(char *path);
int probe_wdl(int *success);
int probe_dtz(int *success);
int root_probe();
int root_probe_wdl();

#endif

