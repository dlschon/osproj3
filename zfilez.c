#include "oufs_lib.h"

int main(int argc, char** argv) 
{
  // Fetch the key environment vars
  char cwd[MAX_PATH_LENGTH];
  char disk_name[MAX_PATH_LENGTH];
  oufs_get_environment(cwd, disk_name);

  // Open virtual disk
  vdisk_disk_open(disk_name);

  oufs_list("/", "/");

  // Close vdisk
  vdisk_disk_close();

  return 0;
}
