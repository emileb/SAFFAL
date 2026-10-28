#include <string>
#include <utility>

/*
     Add a SAF root path to the list of monitored paths
*/
void addSAFPath(std::string safPath);

/*
     Clear all SAF root paths
*/
void clearSAFPaths();

/*
     Remove . ./ ../ etc from a path
*/
std::string getCanonicalPath(std::string path);

/*
    Return true is in SAF area
*/
bool isInSAF(std::string path);

/*
    Virtual working directory, set when chdir() goes into a SAF folder (which the real chdir can't do).
    Empty when the real working directory is in use.
*/
void setVirtualCwd(const std::string &path);
std::string getVirtualCwd();

/*
    getCanonicalPath(), but a relative path is first made absolute against the virtual working
    directory when one is set. 'resolved' is set true when that happened.
*/
std::string getFullPath(const char *path, bool *resolved = nullptr);
