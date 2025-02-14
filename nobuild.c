#define NOBUILD_IMPLEMENTATION
#include "./nobuild.h"
#include <errno.h>


#define CFLAGS "-g", "-Wall", "-Wextra", "-std=c99", "-pedantic"
#define CFLAGS2 "-gdwarf-4", "-Wall", "-Wextra", "-std=c99", "-pedantic"

int main(int argc, char **argv)
{
  GO_REBUILD_URSELF(argc, argv);

  CMD("gcc", CFLAGS, "main.c", "-o", "ftp-server", "-lncurses", "-lcurl");
  // CMD("clang", CFLAGS2,  "main.c", "-o", "ftp-server", "-lncurses", "-lcurl");
  if (argc > 1 && strcmp(argv[1], "upload") == 0)
  {
    if (argc < 2 && argv[2] == NULL)
    {
      fprintf(stderr, "%s Please specify a file to upload!", strerror(errno));
      return 0;
    }
    CMD("./ftp-server", "upload", argv[2], argv[3]);
  }
  else
  {
    CMD("./ftp-server");
  }
  return 0;
}