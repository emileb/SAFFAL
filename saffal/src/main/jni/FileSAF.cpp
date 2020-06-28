#include "FileSAF.h"
#include "FileJNI.h"
#include "Utils.h"

#include <stdio.h>
#include <dlfcn.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <map>

#include <android/log.h>
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO,"FileSAF NDK", __VA_ARGS__))

// Use to disable interception
#if 1

extern "C"
{

// Get the real OS function
	static void* loadRealFunc(const char * name)
	{
		static void * libc = NULL;
		if( libc == NULL )
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
	int open(const char *path, int oflag)
	{
		// Remove relative paths (../ etc)
		std::string fullFilename = getCanonicalPath(path);

		bool inSAF = isInSAF(fullFilename);

		if(inSAF)
		{
			LOGI("open %s", path);

			// fd = -1, not in SAF area, fd = 0, failed to open. Otherwise valid file
			int fd = FileJNI_fopen(fullFilename.c_str(), "r");

			if(fd > 0)
				return fd;
			else
				return -1;
		}
		else // Not in SAF, normal access
		{
			// Load the real function

			static int(*open_real)(const char *path, int oflag) = NULL;

			if(open_real == NULL)
				open_real = (int(*)(const char *path, int oflag))loadRealFunc("__open_2");

			return open_real(path, oflag);
		}
	}

	// Android fcntl.h calls this function instead
	int __open_2(const char *path, int oflag)
	{
		return open(path, oflag);
	}

//------------------------
// fopen INTERCEPT
//------------------------
	/*
	   Check if file is in SAF area and call Java to get FD if so
	*/
	FILE * fopen(const char * filename, const char * mode)
	{
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
			int fd = FileJNI_fopen(fullFilename.c_str(), mode);

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

		static int (*fclose_real)(FILE * file) = NULL;

		if(fclose_real == NULL)
			fclose_real = (int(*)(FILE * file))loadRealFunc("fclose");

		return fclose_real(file);
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
		// LOGI("stat %s", path);

		std::string fullFilename = getCanonicalPath(path);

		bool inSAF = isInSAF(fullFilename);

		if(inSAF)
		{
			// Try to get an fd
			int fd = FileJNI_fopen(fullFilename.c_str(), "r");

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

		bool inSAF = isInSAF(pathname);

		if(inSAF)
		{
			// TODO! Check mode for access bits
			if(FileJNI_exists(pathname))
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
			// Try to get an fd
            int fd = FileJNI_fopen(fullFilename.c_str(), "r");

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

#endif