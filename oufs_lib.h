#ifndef OUFS_LIB
#define OUFS_LIB
#include "oufs.h"

#define MAX_PATH_LENGTH 200

// PROVIDED
void oufs_get_environment(char *cwd, char *disk_name);

// PROJECT 3
int oufs_format_disk(char  *virtual_disk_name);
int oufs_read_inode_by_reference(INODE_REFERENCE i, INODE *inode);
int oufs_write_inode_by_reference(INODE_REFERENCE i, INODE *inode);
int oufs_find_file(char *cwd, char * path, INODE_REFERENCE *parent, INODE_REFERENCE *child, char *local_name);
int oufs_mkdir(char *cwd, char *path);
int oufs_list(char *cwd, char *path);
int oufs_rmdir(char *cwd, char *path);

// Helper functions in oufs_lib_support.c
void oufs_clean_directory_block(INODE_REFERENCE self, INODE_REFERENCE parent, BLOCK *block);
void oufs_clean_directory_entry(DIRECTORY_ENTRY *entry);
BLOCK_REFERENCE oufs_allocate_new_block();

// Helper functions to be provided
int oufs_find_open_bit(unsigned char value);

// PROJECT 4 ONLY
OUFILE* oufs_fopen(char *cwd, char *path, char *mode);
void oufs_fclose(OUFILE *fp);
int oufs_fwrite(OUFILE *fp, unsigned char * buf, int len);
int oufs_fread(OUFILE *fp, unsigned char * buf, int len);
int oufs_remove(char *cwd, char *path);
int oufs_link(char *cwd, char *path_src, char *path_dst);

#endif
