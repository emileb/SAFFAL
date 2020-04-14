#include <string>
#include <utility>

/*
     Set at init to set the SAF path
*/
void setSAFPath( std::string safPath );

/*
     Remove . ./ ../ etc from a path
*/
std::string getCanonicalPath( std::string path );

/*
    Return true is in SAF area
*/
bool isInSAF( std::string path );