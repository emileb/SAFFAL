#include "FileSAF.h"
#include "FileJNI.h"
#include "FileCache.h"
#include "Utils.h"

#include <stdio.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <map>
#include <vector>
#include <set>

#include <android/log.h>
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO,"FileSAF NDK", __VA_ARGS__))

#define LOGI(...)

// Use to disable interception
#if 1

extern "C"
{

// Get the real OS function
	static void* loadRealFunc(const char * name)
	{
		static void * libc = NULL;

		if(libc == NULL)
		{
			libc = dlopen("libc.so", 0);

			if(!libc)
			{
				LOGI("ERROR LIBC NOT LOADED");
			}

			LOGI("fopen = %p", dlsym(libc, "fopen"));
			LOGI("__open_2 = %p", dlsym(libc, "__open_2"));
			LOGI("open = %p", dlsym(libc, "open"));
			LOGI("fclose = %p", dlsym(libc, "fclose"));
			LOGI("stat = %p", dlsym(libc, "stat"));
			LOGI("access = %p", dlsym(libc, "access"));
			LOGI("opendir = %p", dlsym(libc, "opendir"));
		}

		void * func;

		if(!libc) // Could not open libc, bad
			func = dlsym(RTLD_NEXT, name);
		else
			func = dlsym(libc, name);

		if(func == NULL)
		{
			LOGI("ERROR, func %s not loaded, this is bad, really bad, seg fault ahead..", name);
		}

		return func;
	}


//------------------------
// open INTERCEPT
// The only reason this is here is because one engine I use uses 'open' to test the presence of a file
// There could be weird bugs with this..
//------------------------
	int open(const char *path, int oflag, mode_t modes)
	{
		LOGI("open %s, %d %d", path, oflag, modes);

		// Remove relative paths (../ etc)
		std::string fullFilename = getCanonicalPath(path);

		bool inSAF = isInSAF(fullFilename);

		if(inSAF)
		{
			// fd = -1, not in SAF area, fd = 0, failed to open. Otherwise valid file
			int fd = FileCache_getFd(fullFilename.c_str(), "r");

			if(fd > 0)
				return fd;
			else
				return -1;
		}
		else // Not in SAF, normal access
		{
			// Load the real function

			static int(*open_real)(const char *path, int oflag, mode_t modes) = NULL;

			if(open_real == NULL)
				open_real = (int(*)(const char *path, int oflag, mode_t modes))loadRealFunc("open");

			return open_real(path, oflag, modes);
		}
	}

	// Android fcntl.h calls this function instead
	int __open_2(const char *path, int oflag, mode_t modes)
	{
		return open(path, oflag, modes);
	}
/*
	int __open_real(const char *path, int oflag, mode_t modes)
	{
		static int(*open_real)(const char *path, int oflag, mode_t modes) = NULL;

		if(open_real == NULL)
			open_real = (int(*)(const char *path, int oflag, mode_t modes))loadRealFunc("open");

		return open_real(path, oflag, modes);
	}
*/
//------------------------
// fopen INTERCEPT
//------------------------
	/*
	   Check if file is in SAF area and call Java to get FD if so
	*/
	FILE * fopen(const char * filename, const char * mode)
	{
		LOGI("fopen %s", filename);

		if(filename == NULL || mode == NULL)
		{
			LOGI("fopen: filename or mode is null");
			return NULL;
		}

		FILE *file = NULL;

		// Remove relative paths (../ etc)
		std::string fullFilename = getCanonicalPath(filename);

		//LOGI("fopen: file = %s, mode = %s", fullFilename.c_str(), mode);

		// Check if in SAF
		bool inSAF = isInSAF(fullFilename);

		if(inSAF)
		{
			// fd = -1 failed to open. Otherwise valid file
			int fd = FileCache_getFd(fullFilename.c_str(), mode);

			LOGI("fopen: file = %s, mode = %s, fd = %d", fullFilename.c_str(), mode, fd);

			if(fd > 0)   // File was in SAF area
			{
				file = fdopen(fd, mode);

				if(file == NULL)
					LOGI("ERROR fdopen returned NULL");
			}
		}
		else // Not in SAF, normal access
		{
			// Load the real function
			static FILE * (*fopen_real)(const char *, const char *) = NULL;

			if(fopen_real == NULL)
				fopen_real = (FILE * (*)(const char *, const char *))loadRealFunc("fopen");

			file =  fopen_real(filename, mode);
		}

		return file;
	}


//------------------------
// fclose INTERCEPT
// Just used for debug, always use real fclose, even for SAF files
//------------------------
	int fclose(FILE * file)
	{
		LOGI("fclose %p", file);

		FileCache_closeFile(file);

		static int (*fclose_real)(FILE * file) = NULL;

		if(fclose_real == NULL)
			fclose_real = (int(*)(FILE * file))loadRealFunc("fclose");

		return fclose_real(file);
	}

	int close(int fd)
	{
		LOGI("close %d", fd);

		FileCache_closeFd(fd);

		static int (*close_real)(int) = NULL;

		if(close_real == NULL)
			close_real = (int(*)(int))loadRealFunc("close");

		return close_real(fd);
	}

	/*  TODO
	//------------------------
	// mkdir INTERCEPT
	//------------------------
		int mkdir(const char *path, mode_t mode)
		{
			// Remove relative paths (../ etc)
			std::string fullPath = getCanonicalPath(path);

			LOGI("mkdir %s, %d. fullPath %s", path, mode, fullPath.c_str());

			// status = -1, not in SAF area, status = 0 is OK, status = 1 failed
			int status = FileJNI_mkdir(fullPath.c_str());

			if(status == 0)
			{
				return 0; // OK
			}
			else if(status == 1)
			{
				return -1; // Failed
			}
			else // Not in SAF, use real function
			{
				static int (*mkdir_real)(const char *path, mode_t mode) = NULL;

				if(mkdir_real == NULL)
					mkdir_real = (int(*)(const char *path, mode_t mode))loadRealFunc("mkdir");

				return mkdir_real(path, mode);
			}
		}
	*/

//------------------------
// opendir INTERCEPT
//------------------------
	int stat(const char *path, struct stat *statbuf)
	{
		//LOGI("stat %s", path);

		std::string fullFilename = getCanonicalPath(path);

		bool inSAF = isInSAF(fullFilename);

		if(inSAF)
		{
			// Try to get an fd
			int fd = FileCache_getFd(fullFilename.c_str(), "r");

			if(fd > 0)   // File was in SAF area
			{
				fstat(fd, statbuf);
				LOGI("stat size = %ld, mode = %d", statbuf->st_size, statbuf->st_mode);
				close(fd);
				return 0;
			}
			else
			{
				return -1;
			}
		}
		else
		{
			static int(*stat_real)(const char *path, struct stat * statbuf) = NULL;

			if(stat_real == NULL)
				stat_real = (int(*)(const char *path, struct stat * statbuf))loadRealFunc("stat");

			return stat_real(path, statbuf);
		}

	}
//------------------------
// access INTERCEPT
//------------------------
	int access(const char *pathname, int mode)
	{
		LOGI("access %s", pathname);

		std::string fullFilename = getCanonicalPath(pathname);

		bool inSAF = isInSAF(fullFilename);

		if(inSAF)
		{
			// TODO! Check mode for access bits
			if(FileJNI_exists(fullFilename.c_str()))
				return 0;
			else
				return -1;
		}
		else
		{
			static int(*access_real)(const char *pathname, int mode) = NULL;

			if(access_real == NULL)
				access_real = (int(*)(const char *pathname, int mode))loadRealFunc("access");

			return access_real(pathname, mode);
		}
	}


	class DIR_SAF
	{
	public:
		int position;
		std::vector<struct dirent> items;
	};

	std::set<DIR_SAF *> openDIRS;

//------------------------
// opendir INTERCEPT
//------------------------
	DIR *opendir(const char *name)
	{
		LOGI("opendir %s", name);

		std::string fullFilename = getCanonicalPath(name);

		bool inSAF = isInSAF(fullFilename);

		if(inSAF)
		{

#if 0 // FFS, final version of Android 11 breaks this, it only lists directories and no files
			// Try to get an fd
			int fd = FileCache_getFd(fullFilename.c_str(), "r");

			if(fd > 0)   // File was in SAF area
			{
				// YES, surprisingly fdopendir actually works with the fd from SAF.
				// This means I don't need to reimplement it
				DIR* ret = fdopendir(fd);
				return ret;
			}
			else
			{
				return NULL;
			}

#else
			std::vector<std::string> items = FIleJNI_opendir(fullFilename.c_str());

			if(items.size() > 0)
			{
				// Create out own DIR object and fill with the data we need
				DIR_SAF* dirSaf = new DIR_SAF();
				dirSaf->position = 0;

				// Copy the items to out DIR object
				for(auto it = begin(items); it != end(items); ++it)
				{
					struct dirent d = {};

					// The first character of the name is the type (F = file, D = directory)
					std::string type = it->substr(0, 1);
					std::string name = it->substr(1);

					LOGI("Adding %s, type = %s", name.c_str(), type.c_str());

					if(type == "F")
						d.d_type = DT_REG;
					else if(type == "D")
						d.d_type = DT_DIR;
					else
						d.d_type = DT_UNKNOWN;

					// Copy name
					strcpy(d.d_name, name.c_str());

					// Add the item
					dirSaf->items.push_back(d);
				}

				// Add it to our list to remember this is ours
				openDIRS.insert(dirSaf);

				return (DIR *)dirSaf;
			}
			else
			{
				return NULL;
			}

#endif
		}
		else
		{
			static DIR *(*opendir_real)(const char *name) = NULL;

			if(opendir_real == NULL)
				opendir_real = (DIR * (*)(const char *name))loadRealFunc("opendir");

			return opendir_real(name);
		}
	}
}

//------------------------
// opendir INTERCEPT
//------------------------
struct dirent *readdir(DIR *dirp)
{
	LOGI("readdir %p", dirp);

	DIR_SAF* dirSafe = (DIR_SAF*)dirp; // TODO sort out C style casts

	std::set<DIR_SAF *>::iterator it = openDIRS.find(dirSafe);

	if(it != openDIRS.end())    // It is in out list
	{
		if(dirSafe->position < dirSafe->items.size())
		{
			return &dirSafe->items.at(dirSafe->position++);
		}
		else
		{
			return NULL;
		}
	}
	else // Not in our list,  use system the version
	{
		static struct dirent *(*readdir_real)(DIR * dirp) = NULL;

		if(readdir_real == NULL)
			readdir_real = (struct dirent * (*)(DIR * dirp))loadRealFunc("readdir");

		return readdir_real(dirp);
	}
}

//------------------------
// closedir INTERCEPT
//------------------------
int closedir(DIR *dirp)
{
	LOGI("closedir %p", dirp);

	DIR_SAF* dirSafe = (DIR_SAF*)dirp; // TODO sort out C style casts

	std::set<DIR_SAF *>::iterator it = openDIRS.find(dirSafe);

	if(it != openDIRS.end())    // It is in out list
	{
		openDIRS.erase(dirSafe);
		delete dirSafe;
		return 0;
	}
	else // Not in our list, use system the version
	{
		static int (*closedir_real)(DIR * dirp) = NULL;

		if(closedir_real == NULL)
			closedir_real = (int (*)(DIR * dirp))loadRealFunc("closedir");

		return closedir_real(dirp);
	}
}
#endif