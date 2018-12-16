#ifndef TBPROBE_H
#define TBPROBE_H

extern int TBlargest; // 5 if 5-piece tables, 6 if 6-piece tables were found.

void init_tablebases(char *path);
int probe_wdl(int *success, chessposition *pos);
int probe_dtz(int *success, chessposition *pos);
int root_probe(chessposition *pos);
int root_probe_wdl(chessposition *pos);

#endif

