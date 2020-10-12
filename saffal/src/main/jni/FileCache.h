

#include <stdio.h>

int FileCache_getFd(const char * filename, const char * mode);

int FileCache_closeFd(int fd);
int FileCache_closeFile(FILE * file);
