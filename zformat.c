#include "oufs_lib.h"
#include "vdisk.h"

int main()
{
  // Fetch the key environment vars
  char cwd[MAX_PATH_LENGTH];
  char disk_name[MAX_PATH_LENGTH];
  oufs_get_environment(cwd, disk_name);
  
  // Open virtual disk
  vdisk_disk_open(disk_name);

  BLOCK theblock;
  // Write 0 to every block
  for (int i = 0; i < 128; i++)
  {
    vdisk_read_block(i, &theblock);
    memset(&theblock, 0, BLOCK_SIZE);
    vdisk_write_block(i, &theblock);
  }

  // Allocate master block, 8 inode blocks, and first data block
  for (int i = 0; i < N_INODE_BLOCKS + 2; i++)
    oufs_allocate_new_block();

  // Mark first inode as allocated
  INODE_REFERENCE ref = oufs_allocate_new_inode();
  BLOCK_REFERENCE first_block = 1;

  // Set the first inode
  vdisk_read_block(first_block, &theblock);
  theblock.inodes.inode[0].type = IT_DIRECTORY;
  theblock.inodes.inode[0].n_references = 1;
  theblock.inodes.inode[0].data[0] = N_INODE_BLOCKS + 1;
  theblock.inodes.inode[0].size = 2;
  vdisk_write_block(first_block, &theblock);

  // Make the directory in the first open data
  vdisk_read_block(N_INODE_BLOCKS + 1, &theblock);
  oufs_clean_directory_block(ref, ref, &theblock);
  vdisk_write_block(N_INODE_BLOCKS + 1, &theblock);

  return 0;
}