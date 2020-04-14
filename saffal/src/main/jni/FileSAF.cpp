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


extern "C"
{


static void* loadRealFunc( const char * name )
{
    void * func = dlsym( RTLD_NEXT, name );

    if( func == NULL )
    {
        LOGI("ERROR, func %s not loaded, this is bad, seg fault ahead..",name);
    }

    return func;
}

int open(const char *path, int oflag, ... )
{
    LOGI( "open %s", path );

    static int(*open_real)(const char *path, int oflag, ...) = NULL;
    if( open_real == NULL )
        open_real = (int(*)(const char *path, int oflag, ...))loadRealFunc("open");

    return open_real( path, oflag );
}

//------------------------
// fopen INTERCEPT
//------------------------
/*
   Check if file is in SAF area and call Java to get FD if so
*/
FILE * fopen( const char * filename, const char * mode )
{
	LOGI("fopen %s %s", filename, mode );

    if( filename == NULL || mode == NULL )
        return NULL;

	FILE *file = NULL;

	// Remove relative paths (../ etc)
	std::string fullFilename = getCanonicalPath(filename);

	bool inSAF = isInSAF( filename );

	if( inSAF )
	{
	    // fd = -1, not in SAF area, fd = 0, failed to open. Otherwise valid file
	    int fd = FileJNI_fopen( fullFilename.c_str(), mode );

	    if( fd > 0 ) // File was in SAF area
	    {
	        file = fdopen( fd, mode);

	        if( file == NULL )
	            LOGI( "ERROR fdopen returned NULL" );
	    }
	}
	else // Not in SAF, normal access
	{
        // Load the real function
        static FILE * (*fopen_real)(const char *, const char *) = NULL;
        if( fopen_real == NULL )
            fopen_real = (FILE *(*)(const char *, const char *))loadRealFunc("fopen");

		file =  fopen_real( filename, mode );
	}

    return file;
}


//------------------------
// fclose INTERCEPT
// Just used for debug, always use real fclose, even for SAF files
//------------------------
int fclose ( FILE * file )
{
    LOGI("fclose %p", file);

    static int (*fclose_real)(FILE * file) = NULL;
    if( fclose_real == NULL )
        fclose_real = (int(*)(FILE * file))loadRealFunc("fclose");

    return fclose_real( file );
}


//------------------------
// mkdir INTERCEPT
//------------------------
int mkdir(const char *path, mode_t mode)
{

    // Remove relative paths (../ etc)
   	std::string fullPath = getCanonicalPath(path);

	LOGI("mkdir %s, %d. fullPath %s", path, mode, fullPath.c_str());

    // status = -1, not in SAF area, status = 0 is OK, status = 1 failed
    int status = FileJNI_mkdir( fullPath.c_str() );
    if( status == 0 )
    {
        return 0; // OK
    }
    else if( status == 1 )
    {
        return -1; // Failed
    }
    else // Not in SAF, use real function
    {
        static int (*mkdir_real)(const char *path, mode_t mode) = NULL;
        if( mkdir_real == NULL )
            mkdir_real = (int(*)(const char *path, mode_t mode))loadRealFunc("mkdir");

        return mkdir_real( path, mode );
    }
}

//------------------------
// opendir INTERCEPT
//------------------------
int stat(const char *path, struct stat *statbuf)
{
    LOGI( "stat %s", path );

	bool inSAF = isInSAF( path );

	if( inSAF )
	{
		// TODO! No stat information is actually got from SAF yet
		if(FileJNI_exists(path))
			return 0;
		else
			return -1;
	}
	else
	{
		static int(*stat_real)(const char *path, struct stat *statbuf) = NULL;
		if( stat_real == NULL )
		stat_real = (int(*)(const char *path, struct stat *statbuf))loadRealFunc("stat");

		return stat_real( path, statbuf );
    }

}



//------------------------
// opendir INTERCEPT
//------------------------
DIR *opendir(const char *name)
{
    LOGI( "opendir %s", name );

    static DIR *(*opendir_real)(const char *name) = NULL;
    if( opendir_real == NULL )
        opendir_real = (DIR *(*)(const char *name))loadRealFunc("opendir");

    return opendir_real( name );
}

}