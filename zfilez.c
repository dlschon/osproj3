#include "oufs_lib.h"

int main(int argc, char** argv) 
{
  // Fetch the key environment vars
  char cwd[MAX_PATH_LENGTH];
  char disk_name[MAX_PATH_LENGTH];
  oufs_get_environment(cwd, disk_name);

  // Open virtual disk
  vdisk_disk_open(disk_name);

  if (argc == 1)
    // No path supplied, use cwd
    oufs_list(strdup(cwd), "");
  else
  {
    // Path is supplied, so send both the path amd the cwd
    oufs_list(strdup(cwd), strdup(argv[1]));
  }

  // Close vdisk
  vdisk_disk_close();

  return 0;
}
