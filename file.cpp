#include "file.hpp"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <iostream>


using std::string;
using std::cerr;


static bool directoryExists(const string &path)
{
  struct stat info;

  if (stat(path.c_str(),&info)!=0) {
    return false;
  }

  if (info.st_mode & S_IFDIR) {
    return true;
  }

  return false;
}


bool fileExists(const string &path)
{
  struct stat info;

  if (stat(path.c_str(),&info)!=0) {
    return false;
  }

  if (info.st_mode & S_IFREG) {
    return true;
  }

  return false;
}


static size_t lastDirectorySeparatorPosition(const string &path)
{
  return path.rfind('/');
}


string directoryOf(const string &path)
{
  size_t pos = lastDirectorySeparatorPosition(path);

  if (pos==string::npos) {
    return "";
  }

  return path.substr(0,pos);
}


string baseOf(const string &path)
{
  size_t pos = lastDirectorySeparatorPosition(path);

  if (pos==string::npos) {
    return path;
  }

  return path.substr(pos+1);
}


static void createDirectory(const string &path)
{
  if (mkdir(path.c_str(),0777)!=0) {
    cerr << "Unable to create directory " << path << "\n";
  }
}


void createDirectories(const string &path)
{
  if (path=="") return;
  if (directoryExists(path)) return;
  createDirectories(directoryOf(path));
  createDirectory(path);
}
