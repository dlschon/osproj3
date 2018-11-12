#include <stdlib.h>
#include <libgen.h>
#include <string.h>
#include "oufs_lib.h"

#define debug 0

/**
 * Read the ZPWD and ZDISK environment variables & copy their values into cwd and disk_name.
 * If these environment variables are not set, then reasonable defaults are given.
 *
 * @param cwd String buffer in which to place the OUFS current working directory.
 * @param disk_name String buffer containing the file name of the virtual disk.
 */
void oufs_get_environment(char *cwd, char *disk_name)
{
  // Current working directory for the OUFS
  char *str = getenv("ZPWD");
  if(str == NULL) {
    // Provide default
    strcpy(cwd, "/");
  }else{
    // Exists
    strncpy(cwd, str, MAX_PATH_LENGTH-1);
  }

  // Virtual disk location
  str = getenv("ZDISK");
  if(str == NULL) {
    // Default
    strcpy(disk_name, "vdisk1");
  }else{
    // Exists: copy
    strncpy(disk_name, str, MAX_PATH_LENGTH-1);
  }

}

/**
 * Configure a directory entry so that it has no name and no inode
 *
 * @param entry The directory entry to be cleaned
 */
void oufs_clean_directory_entry(DIRECTORY_ENTRY *entry) 
{
  entry->name[0] = 0;  // No name
  entry->inode_reference = UNALLOCATED_INODE;
}

/**
 * Initialize a directory block as an empty directory
 *
 * @param self Inode reference index for this directory
 * @param self Inode reference index for the parent directory
 * @param block The block containing the directory contents
 *
 */

void oufs_clean_directory_block(INODE_REFERENCE self, INODE_REFERENCE parent, BLOCK *block)
{
  // Debugging output
  if(debug)
    fprintf(stderr, "New clean directory: self=%d, parent=%d\n", self, parent);

  // Create an empty directory entry
  DIRECTORY_ENTRY entry;
  oufs_clean_directory_entry(&entry);

  // Copy empty directory entries across the entire directory list
  for(int i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; ++i) {
    block->directory.entry[i] = entry;
  }

  // Now we will set up the two fixed directory entries

  // Self
  strncpy(entry.name, ".", 2);
  entry.inode_reference = self;
  block->directory.entry[0] = entry;

  // Parent (same as self
  strncpy(entry.name, "..", 3);
  entry.inode_reference = parent;
  block->directory.entry[1] = entry;
  
}

/**
 * Allocate a new data block
 *
 * If one is found, then the corresponding bit in the block allocation table is set
 *
 * @return The index of the allocated data block.  If no blocks are available,
 * then UNALLOCATED_BLOCK is returned
 *
 */
BLOCK_REFERENCE oufs_allocate_new_block()
{
  BLOCK block;
  // Read the master block
  vdisk_read_block(MASTER_BLOCK_REFERENCE, &block);

  // Scan for an available block
  int block_byte;
  int flag;

  // Loop over each byte in the allocation table.
  for(block_byte = 0, flag = 1; flag && block_byte < (N_BLOCKS_IN_DISK / 8); ++block_byte) {
    if(block.master.block_allocated_flag[block_byte] != 0xff) {
      // Found a byte that has an opening: stop scanning
      flag = 0;
      break;
    };
  };
  // Did we find a candidate byte in the table?
  if(flag == 1) {
    // No
    if(debug)
      fprintf(stderr, "No blocks\n");
    return(UNALLOCATED_BLOCK);
  }

  // Found an available data block 

  // Set the block allocated bit
  // Find the FIRST bit in the byte that is 0 (we scan in bit order: 0 ... 7)
  int block_bit = oufs_find_open_bit(block.master.block_allocated_flag[block_byte]);

  // Now set the bit in the allocation table
  block.master.block_allocated_flag[block_byte] |= (1 << block_bit);

  // Write out the updated master block
  vdisk_write_block(MASTER_BLOCK_REFERENCE, &block);

  if(debug)
    fprintf(stderr, "Allocating block=%d (%d)\n", block_byte, block_bit);

  // Compute the block index
  BLOCK_REFERENCE block_reference = (block_byte << 3) + block_bit;

  if(debug)
    fprintf(stderr, "Allocating block=%d\n", block_reference);
  
  // Done
  return(block_reference);
}

/**
 * Allocate a new inode block
 *
 * If one is found, then the corresponding bit in the block allocation table is set
 *
 * @return The index of the allocated data block.  If no blocks are available,
 * then UNALLOCATED_BLOCK is returned
 *
 */
BLOCK_REFERENCE oufs_allocate_new_inode()
{
  BLOCK block;
  // Read the master block
  vdisk_read_block(MASTER_BLOCK_REFERENCE, &block);

  // Scan for an available block
  int inode_byte;
  int flag;

  // Loop over each byte in the allocation table.
  for(inode_byte = 0, flag = 1; flag && inode_byte < (N_BLOCKS_IN_DISK / 8); ++inode_byte) {
    if(block.master.inode_allocated_flag[inode_byte] != 0xff) {
      // Found a byte that has an opening: stop scanning
      flag = 0;
      break;
    };
  };
  // Did we find a candidate byte in the table?
  if(flag == 1) {
    // No
    if(debug)
      fprintf(stderr, "No inode\n");
    return(UNALLOCATED_INODE);
  }

  // Found an available data block 

  // Set the block allocated bit
  // Find the FIRST bit in the byte that is 0 (we scan in bit order: 0 ... 7)
  int inode_bit = oufs_find_open_bit(block.master.inode_allocated_flag[inode_byte]);

  // Now set the bit in the allocation table
  block.master.inode_allocated_flag[inode_byte] |= (1 << inode_bit);

  // Write out the updated master block
  vdisk_write_block(MASTER_BLOCK_REFERENCE, &block);

  if(debug)
    fprintf(stderr, "Allocating inode=%d (%d)\n", inode_byte, inode_bit);

  // Compute the block index
  INODE_REFERENCE inode_reference = (inode_byte << 3) + inode_bit;

  if(debug)
    fprintf(stderr, "Allocating inode=%d\n", inode_reference);
  
  // Done
  return(inode_reference);
}

/**
 * Given a block reference, mark that block as unallocated in the master table
 * @param block_ref block reference of block to deallocate
 * @return 0 if success, -1 if error
 */
int oufs_deallocate_block(BLOCK_REFERENCE block_ref)
{
  // Read the master block
  BLOCK block;
  vdisk_read_block(MASTER_BLOCK_REFERENCE, &block);

  // Calculate the byte and bit to change
  int block_bit = block_ref & 0b111;
  int block_byte = block_ref >> 3;

  // Flip the desired bit to 0
  block.master.block_allocated_flag[block_byte] &= ~(1 << block_bit);

  if(debug)
    fprintf(stderr, "Deallocating block=%d (%d)\n", block_byte, block_bit);
  if(debug)
    fprintf(stderr, "Deallocating block=%d\n", block_ref);

  // Write out the updated master block
  vdisk_write_block(MASTER_BLOCK_REFERENCE, &block);

  return 0;
}

/**
 * Given a inode reference, mark that inode as unallocated in the master table
 * @param inode_ref inode reference of inode to deallocate
 * @return 0 if success, -1 if error
 */
int oufs_deallocate_inode(INODE_REFERENCE inode_ref)
{
  // Read the master block
  BLOCK block;
  vdisk_read_block(MASTER_BLOCK_REFERENCE, &block);

  // Calculate the byte and bit to change
  int inode_bit = inode_ref & 0b111;
  int inode_byte = inode_ref >> 3;

  // Flip the desired bit to 0
  block.master.inode_allocated_flag[inode_byte] &= ~(1 << inode_bit);

  if(debug)
    fprintf(stderr, "Deallocating inode=%d (%d)\n", inode_byte, inode_bit);
  if(debug)
    fprintf(stderr, "Deallocating inode=%d\n", inode_ref);

  // Write out the updated master block
  vdisk_write_block(MASTER_BLOCK_REFERENCE, &block);

  return 0;
}

/**
 *  Given an inode reference, read the inode from the virtual disk.
 *
 *  @param i Inode reference (index into the inode list)
 *  @param inode Pointer to an inode memory structure.  This structure will be
 *                filled in before return)
 *  @return 0 = successfully loaded the inode
 *         -1 = an error has occurred
 *
 */
int oufs_read_inode_by_reference(INODE_REFERENCE i, INODE *inode)
{
  if(debug)
    fprintf(stderr, "Fetching inode %d\n", i);

  // Find the address of the inode block and the inode within the block
  BLOCK_REFERENCE block = i / INODES_PER_BLOCK + 1;
  int element = (i % INODES_PER_BLOCK);

  BLOCK b;
  if(vdisk_read_block(block, &b) == 0) {
    // Successfully loaded the block: copy just this inode
    *inode = b.inodes.inode[element];
    return(0);
  }
  // Error case
  return(-1);
}

/**
 *  Given an inode reference, write the inode to the virtual disk.
 *
 *  @param i Inode reference (index into the inode list)
 *  @param inode Pointer to an inode memory structure.  This structure will be
 *                written to the disk)
 *  @return 0 = successfully wrote the inode
 *         -1 = an error has occurred
 *
 */
int oufs_write_inode_by_reference(INODE_REFERENCE i, INODE *inode)
{
  if(debug)
    fprintf(stderr, "Writing inode %d\n", i);

  // Find the address of the inode block and the inode within the block
  BLOCK_REFERENCE block = i / INODES_PER_BLOCK + 1;
  int element = (i % INODES_PER_BLOCK);

  BLOCK b;
  if(vdisk_read_block(block, &b) == 0) {
    // Successfully loaded the block: copy just this inode
    b.inodes.inode[element] = *inode;
    vdisk_write_block(block, &b);
    return(0);
  }
  // Error case
  return(-1);
}

/**
 *  Given a byte, find the first open bit. That is, the first 0 from the right
 *
 *  @param value the byte to check
 *  @return index of the first 0, 0 being the rightmost bit
 *         -1 = an error has occurred
 */
int oufs_find_open_bit(unsigned char value)
{
  int first = 0;

  while (first < 8)
  {
    if (!(1 & value))
      return first;
    else 
    {
      first++;
      value = value >> 1;
    }
  }

	return -1;
}

/**
 * Given the cwd and a path, give a path combining them
 * @param cwd current working directory
 * @param path working path
 * @param rel_path the thing we're outputting
 */
char* oufs_relative_path(char* cwd, char* path, char* rel_path)
{
  if (!strcmp(path, ""))
    // Path is blank so just use the cwd
    strcat(rel_path, cwd);
  else 
  {
    // Path is supplied so use that
    // check if supplied path is relative 
    if (path[0] == '/')
    {
      // path is absolute, no further work need by done
      strcat(rel_path, path);
    }
    else
    {
      // Path is relative. Concat cwd and path into list_dir
      memset(rel_path, 0, MAX_PATH_LENGTH);
      strcat(rel_path, cwd);
      strcat(rel_path, "/");
      strcat(rel_path, path);
    }
  }
}

/**
 *  Format the disk given a virtual disk name
 * 
 *  @param virtual_disk_name name of the virtual disk
 *  @return success code
 */
int oufs_format_disk(char  *virtual_disk_name)
{
  // Open virtual disk
  vdisk_disk_open(virtual_disk_name);

  BLOCK theblock;
  memset(&theblock, 0, BLOCK_SIZE);

  // Write 0 to every block
  for (int i = 0; i < N_BLOCKS_IN_DISK; i++)
  {
    vdisk_write_block(i, &theblock);
  }

  // Allocate master block
  oufs_allocate_new_block();

  // Allocate 8 inode blocks, but not the inodes
  for (int i = 0; i < N_INODE_BLOCKS; i++)
    oufs_allocate_new_block();

  // Allocate first data block
  BLOCK_REFERENCE first_data_block = oufs_allocate_new_block();

  // Allocate the first inode
  INODE_REFERENCE ref = oufs_allocate_new_inode();
  BLOCK_REFERENCE first_block = 1;

  // Set the first inode
  vdisk_read_block(first_block, &theblock);
  theblock.inodes.inode[0].type = IT_DIRECTORY;
  theblock.inodes.inode[0].n_references = 1;
  theblock.inodes.inode[0].data[0] = N_INODE_BLOCKS + 1;
  for (int i = 1; i < BLOCKS_PER_INODE; i++)
    theblock.inodes.inode[0].data[i] = UNALLOCATED_BLOCK;
  theblock.inodes.inode[0].size = 2;
  vdisk_write_block(first_block, &theblock);

  // Make the directory in the first open data
  vdisk_read_block(first_data_block, &theblock);
  oufs_clean_directory_block(ref, ref, &theblock);
  vdisk_write_block(first_data_block, &theblock);

  // Close the virtual disk
  vdisk_disk_close();

  return 0;
}

/**
 * Tries to get a file in the file system
 * @param cwd current working directory
 * @param path absolute or relative path of file to look for
 * @param parent parent inode of the found file (output)
 * @param child inode of the found file
 * @param local_name name of the found file
 * @return 1 if the file was found, 0 if not
 */

int oufs_find_file(char *cwd, char * path, INODE_REFERENCE *parent, INODE_REFERENCE *child, char *local_name)
{
  // Find the directory to list, either a supplied path or the cwd
  char listdir[MAX_PATH_LENGTH];
  memset(listdir, 0, MAX_PATH_LENGTH);
  oufs_relative_path(cwd, path, listdir);

  // Declare some variables
  BLOCK_REFERENCE current_block = N_INODE_BLOCKS + 1;
  BLOCK theblock;
  INODE_REFERENCE ref = 0;
  INODE_REFERENCE lastref = 0;

  // Tokenize the path
  char* token = strtok(listdir, "/");
  char lasttoken[FILE_NAME_SIZE];
  memset(lasttoken, '\0', FILE_NAME_SIZE);
  lasttoken[0] = '/';
  while (token != NULL)
  {
    // Check if the expected token exists in this directory
    int flag = 0;
    vdisk_read_block(current_block, &theblock);
    for (int i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; i++)
    {
      if (theblock.directory.entry[i].inode_reference != UNALLOCATED_INODE)
      {
        if (!strcmp(theblock.directory.entry[i].name, token))
        {
          // found it!
          flag = 1;
          lastref = ref;
          ref = theblock.directory.entry[i].inode_reference;

          // load the inode
          INODE inode;
          oufs_read_inode_by_reference(ref, &inode);

          // Grab the next level block reference
          current_block = inode.data[0];
        }
      }
    }

    if (flag == 0)
    {
      if (debug)
        fprintf(stderr, "find_file: directory does not exist\n");
      return 0;
    }

    // Save the token into the last token variable
    memset(lasttoken, '\0', FILE_NAME_SIZE);
    strncpy(lasttoken, token, FILE_NAME_SIZE);
    // Try to get the next token
    token = strtok(NULL, "/");

  } // end while

  // We're at the end of the path and we have presumably found the file. set the return values
  *child = ref;
  *parent = lastref;
  local_name = lasttoken;

  if (debug)
  {
    fprintf(stderr, "findfile: child - %d\n", *child);
    fprintf(stderr, "findfile: parent - %d\n", *parent);
    fprintf(stderr, "findfile: local name - %s\n", local_name);
  }

  return 1;
}

/* qsort C-string comparison function */ 
int cstring_cmp(const void *a, const void *b) 
{ 
    const char **ia = (const char **)a;
    const char **ib = (const char **)b;
    return strcmp(*ia, *ib);
	/* strcmp functions works exactly as expected from
	comparison function */ 
} 

/**
 * List the files in a directory in alphabetical order
 * @param cwd current working directory
 * @param path of the directory to list
 * @return 0 if success, -1 if error
 */
int oufs_list(char *cwd, char *path)
{
  // Declare some variables which will be assigned by find_file
  INODE_REFERENCE child;
  INODE_REFERENCE parent;
  char* local_name;

  // Find the file
  if (!oufs_find_file(cwd, path, &parent, &child, local_name))
  {
    if (debug)
      fprintf(stderr, "zfilez: directory does not exist!\n");
    return -1;
  }

  // get inode object
  INODE inode;
  oufs_read_inode_by_reference(child, &inode);

  // Get data block pointed to by inode
  BLOCK_REFERENCE blockref = inode.data[0];
  BLOCK theblock;
  vdisk_read_block(blockref, &theblock);

  // we're at the end of the path, so list the things
  char* filelist[DIRECTORY_ENTRIES_PER_BLOCK];
  int numFiles = 0;
  for (int i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; i++)
  {
    if (theblock.directory.entry[i].inode_reference != UNALLOCATED_INODE)
    {
      // Add name to the list
      filelist[numFiles] = theblock.directory.entry[i].name;
      numFiles++;
    }
  }

  // Sort list of names
  qsort(filelist, numFiles, sizeof(char*), cstring_cmp);

  // Print the sorted list
  for (int i = 0; i < numFiles; i++)
  {
    printf("%s/\n", filelist[i]);
  }

  return 0;
}

/**
 * makes a directory
 * @param cwd current working directory
 * @param path path to create
 * @return status code
 */
int oufs_mkdir(char *cwd, char *path)
{
  // Get relative path
  char rel_path[MAX_PATH_LENGTH];
  memset(rel_path, 0, MAX_PATH_LENGTH);
  oufs_relative_path(cwd, path, rel_path);

  // Get base and directory names
  char* dir = dirname(strdup(rel_path));
  char* base = basename(strdup(rel_path));

  // Find file outputs
  INODE_REFERENCE parent;
  INODE_REFERENCE child;
  char* local_name;

  INODE_REFERENCE new_dir_parent;

  // Parent directory must exist
  if (!oufs_find_file(cwd, dir, &parent, &child, local_name))
  {
      // Parent directory does not exist
      if (debug)
        fprintf(stderr, "mkdir: Parent directory does not exist!\n");
      return -1;
  }
  else
    new_dir_parent = child;

  // Child directory must not exist
  if (oufs_find_file(cwd, rel_path, &parent, &child, local_name))
  {
      // Directory we are trying to make already exists
      if (debug)
        fprintf(stderr, "mkdir: Directory already exists\n");
      return -1;
  }

  // Allocated the new block
  BLOCK_REFERENCE new_dir_block_ref = oufs_allocate_new_block();

  // Make a new inode for the new directory
  INODE_REFERENCE new_inode_ref = oufs_allocate_new_inode();
  if (debug)
    fprintf(stderr, "new inode ref: %d\n", new_inode_ref);

  // Set the inode for the new directory
  INODE new_inode;
  oufs_read_inode_by_reference(new_inode_ref, &new_inode);
  new_inode.type = IT_DIRECTORY;
  new_inode.n_references = 1;
  new_inode.data[0] = new_dir_block_ref;
  for (int i = 1; i < BLOCKS_PER_INODE; i++)
    new_inode.data[i] = UNALLOCATED_BLOCK;
  new_inode.size = 2;
  oufs_write_inode_by_reference(new_inode_ref, &new_inode);

  // Clean the directory
  BLOCK theblock;
  vdisk_read_block(new_dir_block_ref, &theblock);
  oufs_clean_directory_block(new_inode_ref, new_dir_parent, &theblock);
  vdisk_write_block(new_dir_block_ref, &theblock);

  // Update entries in parent block
  INODE parent_inode;
  oufs_read_inode_by_reference(new_dir_parent, &parent_inode);
  BLOCK_REFERENCE parent_block_ref = parent_inode.data[0];
  vdisk_read_block(parent_block_ref, &theblock);

  // Find the first available entry in the block
  int wrote_entry = 0;
  for (int i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; i++)
  {
    // Is the directory entry unallocated?
    if (theblock.directory.entry[i].inode_reference == UNALLOCATED_INODE)
    {
      // Set the empty entry to point to our new inode
      memset(theblock.directory.entry[i].name, '\0', FILE_NAME_SIZE);
      strncpy(theblock.directory.entry[i].name, base, FILE_NAME_SIZE-1);
      theblock.directory.entry[i].inode_reference = new_inode_ref;
      wrote_entry = 1;
      vdisk_write_block(parent_block_ref, &theblock);

      // Update file count in inode
      parent_inode.size++;
      oufs_write_inode_by_reference(new_dir_parent, &parent_inode);

      // Exit the for loop
      break;
    }
  }
  
  if (!wrote_entry)
  {
    if (debug)
      fprintf(stderr, "Directory is full!");
    return -1;
  }

  return 0;
}

/**
 * Removes a directory
 * @param cwd current working directory
 * @param path path to create
 * @return status code
 */
int oufs_rmdir(char *cwd, char *path)
{
  // Get relative path
  char rel_path[MAX_PATH_LENGTH];
  memset(rel_path, 0, MAX_PATH_LENGTH);
  oufs_relative_path(cwd, path, rel_path);

  // Get base and directory names
  char* dir = dirname(strdup(rel_path));
  char* base = basename(strdup(rel_path));

  // Find file outputs
  INODE_REFERENCE parent_inode_ref;
  INODE_REFERENCE child_inode_ref;
  char* local_name;

  // Directory must exist
  if (!oufs_find_file(cwd, rel_path, &parent_inode_ref, &child_inode_ref, local_name))
  {
      // Directory we are trying to make already exists
      if (debug)
        fprintf(stderr, "rmdir: Directory does not exist\n");
      return -1;
  }

  // Get the inode object for the child directory
  INODE child_inode;
  oufs_read_inode_by_reference(child_inode_ref, &child_inode);

  // Inode must point to a directory
  if (child_inode.type != IT_DIRECTORY)
  {
    if (debug)
      fprintf(stderr, "rmdir: path must be a directory\n");
    return -1;
  }

  // Directory must be empty
  if (child_inode.size > 2)
  {
    if (debug)
      fprintf(stderr, "rmdir: cannot remove non-empty directory\n");
    return -1;
  }

  // Directory must not be . or ..
  if (!strcmp(local_name, ".") || !strcmp(local_name, ".."))
  {
    if (debug)
      fprintf(stderr, "rmdir: cannot remove . or ..\n");
    return -1;
  }

  // Deallocate the block and inode in the master block
  oufs_deallocate_inode(child_inode_ref);
  BLOCK_REFERENCE child_block_ref = child_inode.data[0];
  oufs_deallocate_block(child_block_ref);

  // Remove inode properties
  child_inode.data[0] = 0;
  child_inode.type = IT_NONE;
  oufs_write_inode_by_reference(child_inode_ref, &child_inode);

  // Read data for parent of deleted directory
  INODE parent_inode;
  oufs_read_inode_by_reference(parent_inode_ref, &parent_inode);
  BLOCK_REFERENCE parent_block_ref = parent_inode.data[0];
  BLOCK parent_block;
  vdisk_read_block(parent_block_ref, &parent_block);

  // Remove the directory's entry from its parent directory
  int removed_entry = 0;
  for (int i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; i++)
  {
    // Does the directory entry point to the one we're deleting?
    if (parent_block.directory.entry[i].inode_reference == child_inode_ref)
    {
      // Set the empty entry to point to our new inode
      strncpy(parent_block.directory.entry[i].name, "", FILE_NAME_SIZE);
      parent_block.directory.entry[i].inode_reference = UNALLOCATED_INODE;
      removed_entry = 1;
      vdisk_write_block(parent_block_ref, &parent_block);

      // Update file count in inode
      parent_inode.size--;
      oufs_write_inode_by_reference(parent_inode_ref, &parent_inode);

      // Exit the for loop
      break;
    }
  }

  // Reset all directory entries in child
  BLOCK child_block;
  vdisk_read_block(child_block_ref, &child_block);
  for (int i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; i++)
  {
    strncpy(child_block.directory.entry[i].name, "", FILE_NAME_SIZE);
    child_block.directory.entry[i].inode_reference = UNALLOCATED_INODE;

    child_inode.size = 0;
    vdisk_write_block(child_block_ref, &child_block);
  }
  
  if (!removed_entry)
  {
    if (debug)
      fprintf(stderr, "rmdir: failed to remove entry from parent\n");
    return -1;
  }

  return 0;
}
