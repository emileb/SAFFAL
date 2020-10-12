
#include "FileCache.h"
#include "FileJNI.h"

#include <sys/types.h>
#include <unistd.h>

#include <map>
#include <string>
#include <vector>

#include <android/log.h>
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO,"FileCache NDK", __VA_ARGS__))


class CachedItem
{
public:
	//std::string path;
	std::vector<int> fds;
};

static std::map<int, std::string> cacheActive;

static std::map<std::string, CachedItem> cacheFree;


int FileCache_getFd(const char * filename, const char * mode)
{

	int fd = 0;

	//LOGI("FileCache_getFd %s, %s", filename, mode);

	// Check if file is in our cache
	if(cacheFree.find(filename) == cacheFree.end())      // not found
	{
		//LOGI("FileCache_getFd NOT FOUND");
		fd = FileJNI_fopen(filename, mode);

		if(fd > 0)
		{
			LOGI("FileCache_getFd %s(%s) NOT FOUND, new fd = %d", filename, mode, fd);
		}
	}
	else  // found
	{

		// Read fd
		fd = cacheFree[filename].fds.at(0);
		LOGI("FileCache_getFd %s(%s) FOUND, fd = %d", filename,mode, fd);
		// Remove the fd from the free cache
		cacheFree[filename].fds.erase(cacheFree[filename].fds.begin());

		if(cacheFree[filename].fds.size() == 0)
		{
			cacheFree.erase(filename);
		}

		// Reset the fd position
		lseek(fd, 0, SEEK_SET);
	}

	//LOGI("FileCache_getFd fd = %d", fd);

	// FD should always be unique, so map works
	if(fd > 0)
	{
		cacheActive[fd] = filename;
	}

	return fd;
}

static void closeFd(int fd)
{
	if(cacheActive.find(fd) == cacheActive.end())      // not found
	{
		LOGI("FileCache_closeFile NOT FOUND");
	}
	else
	{
		// Make a copy of the FD so the one held in FILE can be close
		int fdCopy = dup(fd);

		LOGI("FileCache_closeFile FOUND  %s", cacheActive[fd].c_str());

		if(cacheFree.find(cacheActive[fd]) == cacheFree.end())      // not found
		{
			LOGI("FileCache_closeFile NEW %d", fdCopy);

			CachedItem newItem;
			newItem.fds.push_back(fdCopy);
			cacheFree[cacheActive[fd]] = newItem;
		}
		else
		{
			LOGI("FileCache_closeFile UPDATE fd = %d", fdCopy);

			cacheFree[cacheActive[fd]].fds.push_back(fdCopy);
		}

		cacheActive.erase(fd);
	}
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


