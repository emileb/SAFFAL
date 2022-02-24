
#include "FileCache.h"
#include "FileJNI.h"

#include <sys/types.h>
#include <unistd.h>

#include <map>
#include <string>
#include <vector>

#include <pthread.h>

#include <android/log.h>
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO,"FileCache NDK", __VA_ARGS__))

#define LOGI(...)


static pthread_mutex_t lock;

#if 1
#define MUTEX_LOCK  pthread_mutex_lock(&lock);
#define MUTEX_UNLOCK  pthread_mutex_unlock(&lock);
#else
#define MUTEX_LOCK
#define MUTEX_UNLOCK
#endif


static std::map<int, std::string> cacheActive;
static std::map<std::string, int> cacheFree;

void FileCache_init()
{
	pthread_mutex_init(&lock, NULL);
}
#include <sys/types.h>

int FileCache_getFd(const char * filename, const char * mode, int (*openFunc)(const char * filename, const char * mode))
{
	MUTEX_LOCK

//	pid_t tid = gettid();
//	LOGI("tid = %d", tid);

	int fd = 0;

	static char fileTag[256]; // Need to include the filename AND the mode, can not mix modes
	snprintf(fileTag, 256, "%s - %s", filename, mode);

	//LOGI("FileCache_getFd %s, %s", filename, mode);

	// Check if file is in our cache
	if(cacheFree.find(fileTag) == cacheFree.end())      // not found
	{
		//LOGI("FileCache_getFd NOT FOUND");
		fd = openFunc(filename, mode);

		if(fd > 0)
		{
			LOGI("FileCache_getFd %s(%s) NOT FOUND, new fd = %d", filename, mode, fd);
		}
	}
	else  // found
	{
		// Get cached fd
		fd = cacheFree[fileTag];

		//Remove from free cache
		cacheFree.erase(fileTag);

		LOGI("FileCache_getFd %s(%s) FOUND, fd = %d", filename, mode, fd);

		// Reset the fd position
		lseek(fd, 0, SEEK_SET);
	}

	//LOGI("FileCache_getFd fd = %d", fd);

	// FD should always be unique, so map works
	if(fd > 0)
	{
		cacheActive[fd] = fileTag;
	}

	MUTEX_UNLOCK

	return fd;
}

static void closeFd(int fd)
{
	MUTEX_LOCK

	if(cacheActive.find(fd) == cacheActive.end())      // not found
	{
		LOGI("FileCache_closeFile NOT FOUND");
	}
	else
	{
		LOGI("FileCache_closeFile FOUND  %s", cacheActive[fd].c_str());

#if 1
		// If we havn't already got a cached of this file, add it now
		if(cacheFree.find(cacheActive[fd]) == cacheFree.end())
		{
			// Make a copy of the FD so the one held in FILE can be close
			int fdCopy = dup(fd);

			// Save new fd to the free cache
			LOGI("FileCache_closeFile CACHEING %d", fdCopy);

			cacheFree[cacheActive[fd]] = fdCopy;
		}
#endif
		// Remove the old fd from the current active cache
		cacheActive.erase(fd);
	}

	MUTEX_UNLOCK
}

int FileCache_closeFd(int fd)
{
	closeFd(fd);
	return 0;
}

int FileCache_closeFile(FILE * file)
{
	int fd = fileno(file);
	closeFd(fd);
	return 0;
}


