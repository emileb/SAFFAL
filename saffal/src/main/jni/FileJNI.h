
#include <vector>


int FileJNI_fopen(const char * filename, const char * mode);
int FileJNI_fclose(int fd);
int FileJNI_mkdir(const char * path);
int FileJNI_exists(const char * path);

std::vector<std::string> FIleJNI_opendir(const char * path);
