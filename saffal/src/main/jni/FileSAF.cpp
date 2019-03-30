#include "FileSAF.h"
#include "FileJNI.h"

#include <stdio.h>
#include <dlfcn.h>
#include <dirent.h>

#include <string>
#include <map>

#include <android/log.h>
#define LOGI(...) ((void)__android_log_print(ANDROID_LOG_INFO,"FileSAF NDK", __VA_ARGS__))

static std::string m_cwd = "";

extern "C"
{


// This is used to store the file pointers opened by SAF so they can be closed by SAF on fclose
static std::map<FILE *, int> m_openSAFFiles;


static std::string getFullPath( const char * path )
{
    std::string fullPath;

    if(path[0] == '/') // Full path
    {
        fullPath = path;
    }
    else // use CWD
    {
        fullPath = m_cwd + "/" + path;
    }

    return fullPath;
}

static void* loadRealFunc( const char * name )
{
    void * func = dlsym( RTLD_NEXT, name );

    if( func == NULL )
    {
        LOGI("ERROR, func %s not loaded, this is bad, seg fault ahead..",name);
    }

    return func;
}

//------------------------
// fopen INTERCEPT
//------------------------
/*
    Builds full path from CWD, calls JAVA SAF code to check if in the SAF area, open if so.
    If not in SAF area open as normal.
*/
FILE * fopen( const char * filename, const char * mode )
{
    if( filename == NULL || mode == NULL )
        return NULL;

    LOGI("fopen %s %s", filename,mode);

    std::string fullFilename = getFullPath( filename );

    LOGI( "fopen full path = %s", fullFilename.c_str() );

    // fd = -1, not in SAF area, fd = 0, failed to open. Otherwise valid file
    int fd = FileJNI_fopen( fullFilename.c_str(), mode );

    if( fd > 0 ) // File was in SAF area
    {
        FILE *file = fdopen( fd, mode);

        if( file == NULL )
            LOGI( "ERROR fdopen returned NULL" );

        // Add SAF file to the map so we can find it again on fclose
        m_openSAFFiles.insert(std::pair<FILE *, int>(file, fd));

        return file;
    }
    else if( fd == -1 ) // Not in SAF, use real function
    {
        // Load the real function
        static FILE * (*fopen_real)(const char *, const char *) = NULL;
        if( fopen_real == NULL )
            fopen_real = (FILE *(*)(const char *, const char *))loadRealFunc("fopen");

        return fopen_real( filename, mode );
    }
    else // fd == 0, failed to open from SAF area
    {
        return NULL;
    }
}


//------------------------
// fclose INTERCEPT
//------------------------
int fclose ( FILE * file )
{
    LOGI("fclose %p", file);

    // Check if file is in the SAF map
    if( m_openSAFFiles.count( file ) != 0 )
    {
        int fd = m_openSAFFiles.at( file );
        FileJNI_fclose( fd );
        LOGI("FileJNI_fclose done");

        // Remove from the list
        m_openSAFFiles.erase( file );

        return 0;
    }
    else // Nope, use normal filesystem
    {
        static int (*fclose_real)(FILE * file) = NULL;
        if( fclose_real == NULL )
            fclose_real = (int(*)(FILE * file))loadRealFunc("fclose");

        return fclose_real( file );
    }
}

//------------------------
// chdir INTERCEPT
//------------------------
int chdir(const char *path)
{
    LOGI("chdir %s", path);

    // Possible to give relative path
    std::string fullPath = getFullPath( path );

    LOGI( "chdir full path = %s", fullPath.c_str() );

    // Save the CWD in order to rebuild full path
    m_cwd = fullPath;

    // TODO, only change dir if not in SAF

    static int (*chdir_real)(const char *path) = NULL;
    if( chdir_real == NULL )
        chdir_real = (int(*)(const char *path))loadRealFunc("chdir");

    return chdir_real( m_cwd.c_str() );
}
//------------------------
// getcwd INTERCEPT
//------------------------
char *getcwd(char *buf, size_t size)
{
    snprintf( buf, size, "%s", m_cwd.c_str() );
    return buf;
}


//------------------------
// mkdir INTERCEPT
//------------------------
int mkdir(const char *path, mode_t mode)
{
    LOGI("mkdir %s, %d", path, mode);

   // Possible to give relative path
    std::string fullPath = getFullPath( path );

    LOGI( "mkdir full path = %s", fullPath.c_str() );



    static int (*mkdir_real)(const char *path, mode_t mode) = NULL;
    if( mkdir_real == NULL )
        mkdir_real = (int(*)(const char *path, mode_t mode))loadRealFunc("mkdir");

    return mkdir_real( path, mode );

}

}