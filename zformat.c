#include "oufs_lib.h"
#include "vdisk.h"

int main()
{
  // Fetch the key environment vars
  char cwd[MAX_PATH_LENGTH];
  char disk_name[MAX_PATH_LENGTH];
  oufs_get_envirnment(cwd, disk_name);
  
  // Open virtual disk
  vdisk_disk_open(disk_name);

  // Write 0 to every block
  for (int i = 0; i < 128; i++)
  {
    block_u theblock;
    vdisk_read_block(i, theblock)
    memset(&theblock, 0, BLOCK_SIZE)
    vdisk_write_block(i, theblock)
  }

  // Allocate master block, 8 inode blocks, and first data block
  for (int i = 0; i < 10; i++)
    oufs_allocate_new_block();

  // Mark first inode as allocated
  oufs_allocate_new_inode();

  // 

  return 0;
}