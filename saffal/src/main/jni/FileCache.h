

#include <stdio.h>

void FileCache_init();

void FileCache_setMaxCached(int max);

int FileCache_getFd(const char * filename, const char * mode, int (*openFunc)(const char * filename, const char * mode));

int FileCache_closeFd(int fd);
int FileCache_closeFile(FILE * file);
