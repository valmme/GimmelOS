#ifndef GOS_SHELL_H
#define GOS_SHELL_H

void shell_init(int wid);
void shell_update(int wid);
void shell_draw(int wid);

void sp(const char* s);
void cmd_lito(const char* args);

#endif // GOS_SHELL_H