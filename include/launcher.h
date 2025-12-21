#ifndef LAUNCHER_H
#define LAUNCHER_H

void handleSingleplay(void);
void handleMultihost(void);
void handleMultijoin(void);
void cleanServer(void);
void gameResult(const char* temp_file, const char* player_name, const char* mode_str);

#endif