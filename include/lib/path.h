#pragma once
#include "types.h"
#include "fs.h"
#include "kstring.h"
#define PATH_MAX 512

extern int32_t cwd_inode;
extern char cwd_path[PATH_MAX];

void path_push(char* path, const char* seg);
void path_pop(char* path);
int resolve_path(const char* path);