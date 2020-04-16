
extern "C"
{
	int FileJNI_fopen(const char * filename, const char * mode);
	int FileJNI_fclose(int fd);
	int FileJNI_mkdir(const char * path);
	int FileJNI_exists(const char * path);
}