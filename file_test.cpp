#include "file.hpp"

#include <cassert>


int main()
{
  assert(directoryOf("dir1/dir2")=="dir1");
  assert(directoryOf("dir1")=="");
  assert(baseOf("dir1")=="dir1");
  assert(baseOf("dir1/dir2")=="dir2");
}
