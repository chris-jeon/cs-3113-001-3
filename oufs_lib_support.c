#include <stdlib.h>
#include "oufs_lib.h"

#include <libgen.h>
#include <string.h>

#define debug 0

int oufs_print_bin(char bin);

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
	} else {
		// Exists
		strncpy(cwd, str, MAX_PATH_LENGTH-1);
	}

	// Virtual disk location
	str = getenv("ZDISK");
	if(str == NULL) {
		// Default
		strcpy(disk_name, "vdisk1");
	} else {
		// Exists: copy
		strncpy(disk_name, str, MAX_PATH_LENGTH-1);
	}

}

/**
 * Configure a directory entry so that it has no name and no inode
 *
 * @param entry The directory entry to be cleaned
 */
void oufs_clean_directory_entry(DIRECTORY_ENTRY * entry) 
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
	for(block_byte = 0, flag = 1; flag && block_byte < N_BLOCKS_IN_DISK / 8; ++block_byte) {
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

int oufs_format_disk(char * virtual_disk_name){

	// Zero out entire virtual disk
	BLOCK block;
	memset(&block, 0, sizeof(block));
	for (int i = 0; i < N_BLOCKS_IN_DISK; i++) {
		vdisk_write_block(i, &block);
	}

	// Format master block
	memset(&block, 0, sizeof(block));
	block.master.inode_allocated_flag[0] = 1;
	for (int i = 1; i <= 6; i++){
		block.master.inode_allocated_flag[i] = 0;
	}
	block.master.block_allocated_flag[0] = 255;
	block.master.block_allocated_flag[1] = 3;
	if (vdisk_write_block(0, &block) < 0)
		fprintf(stderr, "Unable to format master block\n");

	// Format inode[0]
	memset(&block, 0, sizeof(block));
	block.inodes.inode[0].type = IT_DIRECTORY;
	block.inodes.inode[0].n_references = 1;
	for (int i = 0; i < BLOCKS_PER_INODE; i++) {
		if (i == 0)
			block.inodes.inode[0].data[i] = 9;
		else
			block.inodes.inode[0].data[i] = UNALLOCATED_BLOCK;
	}
	block.inodes.inode[0].size = 2;
	if (vdisk_write_block(1, &block) < 0)
		fprintf(stderr, "Unable to format master block\n");

	// Format root directory
	memset(&block, 0, sizeof(block));
	strcpy(block.directory.entry[0].name, ".");
	strcpy(block.directory.entry[1].name, "..");
	block.directory.entry[0].inode_reference = 0;
	block.directory.entry[1].inode_reference = 0;
	for (int i = 2; i < DIRECTORY_ENTRIES_PER_BLOCK; i++)
		block.directory.entry[i].inode_reference = UNALLOCATED_INODE;
	if (vdisk_write_block(9, &block) < 0)
		fprintf(stderr, "Unable to format master block\n");

	return 0;
}

// Working
int oufs_print_bin(char bin) {

	for (int i = 0; i < 8; i++) {
		if (fprintf(stdout, "%d", !!((bin << i) & 0x80)) == -1){
			fprintf(stderr, "Unable to print char in binary oufs_print_bin(%c)\n", bin);
			return -1;
		}
	}
	fprintf(stdout, "\n");

	return 0;
}

// Working
int oufs_write_inode_by_reference(INODE_REFERENCE i, INODE * inode){

	if(debug)
		fprintf(stderr, "Fetching inode %d\n", i);

	int block_no = (i/INODES_PER_BLOCK) + 1;
	int inode_no = i % INODES_PER_BLOCK;

	BLOCK block;
	if (vdisk_read_block(block_no, &block) == -1) return -1;

	block.inodes.inode[inode_no] = *inode;

	if (vdisk_write_block(block_no, &block) == -1) return -1;

	return 0;
}

// Given a directory inode, find the entry with a specified name and return its inode index
int oufs_find_inode_ref_by_name (INODE inode, char * name, INODE_REFERENCE * inode_reference) {

	BLOCK block;

	// Read directory block
	if (vdisk_read_block(inode.data[0], &block) == -1)
		return -1;


	// Loop through directory entries
	for (int i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; i++) {

		// If we find a matching directory entry...
		if (!strcmp(block.directory.entry[i].name, name)){

			// Make note of the entry's inode reference and break out of the loop
			*inode_reference = block.directory.entry[i].inode_reference;
			return 1;
		}
	}

	// No match was found
	return 0;
}

int oufs_find_file (char * cwd, char * path, INODE_REFERENCE * parent, INODE_REFERENCE * child) {

	INODE_REFERENCE cwd_inode_ref = 0;
	INODE_REFERENCE path_inode_ref;

	if (!strcmp(path, "/")) {
		fprintf(stderr, "Improper path name %s\n", path);
		return -1;
	}

	if (strcmp(cwd, "/")) { // If the cwd is not the home directory

		char ** cwd_ptr;
		char * cwd_tok[MAX_PATH_LENGTH];                     // pointers to cwd_ptr strings
		char cwd_buf[MAX_PATH_LENGTH];                      // line buffer
		strncpy(cwd_buf, cwd, MAX_PATH_LENGTH);
		cwd_ptr = cwd_tok;
		*cwd_ptr++ = strtok(cwd_buf,"/");   // tokenize input
		while ((*cwd_ptr++ = strtok(NULL,"/")));
		cwd_ptr = cwd_tok;

		INODE cwd_inode;

		for (;;) {

			// Load new inode
			memset(&cwd_inode, 0, sizeof(cwd_inode));
			oufs_read_inode_by_reference(cwd_inode_ref, &cwd_inode);

			if (oufs_find_inode_ref_by_name (cwd_inode, *cwd_ptr, &cwd_inode_ref)) {
				*parent = *child; // Assign the parent the the child's previous inode ref
				*child = cwd_inode_ref; // Assign the child to the newly found inode ref
				cwd_ptr = cwd_ptr + 1; // Move to the next token
				if (!*cwd_ptr) {
					// Valid cwd path
					path_inode_ref = cwd_inode_ref;
					break;
				}
			} else {
				fprintf(stderr, "Invalid cwd %s\n", cwd);
				return -1;
			}
		}
	}


	// Since the cwd inode was properly found, tokenize path
	char path_buf[MAX_PATH_LENGTH];                      // line buffer
	char * path_tok[MAX_PATH_LENGTH];                     // pointers to path_ptr strings
	char ** path_ptr;
	strncpy(path_buf, path, MAX_PATH_LENGTH);
	path_ptr = path_tok;
	*path_ptr++ = strtok(path_buf,"/");   // tokenize input
	while ((*path_ptr++ = strtok(NULL,"/")));
	path_ptr = path_tok;

	INODE path_inode;

	for (;;) {

		// Load new inode
		memset(&path_inode, 0, sizeof(path_inode));
		oufs_read_inode_by_reference(path_inode_ref, &path_inode);

		if (oufs_find_inode_ref_by_name (path_inode, *path_ptr, &path_inode_ref)) {
			// Match found at current inode
			*parent = *child;
			*child = path_inode_ref;
			path_ptr = path_ptr + 1; // Move to the next token
			if (!*path_ptr) {
				// Name found
				return 1;
			}
		} else {
			path_ptr = path_ptr + 1;
			if (!*path_ptr) {
				// Name not found
				return 0;
			} else {
				// Invalid path
				fprintf(stderr, "Improper path name %s\n", path);
				return -1;
			}
		}

	}

	// This should not be reached
	return -1;
}

// Working
int oufs_flip_bit(char * byte, int pos) {

	int mask = 1 << pos;
	*byte ^= mask;

	return 0;
}

// Working
int oufs_find_available_bit(char byte, int * pos) {

	for (int i = 0; i < 8; i++){
		if (((byte >> i) & 0x01) == 0){
			*pos = i;
			// Return 1 if position is found
			return 1;
		}
	}

	// Return 0 if nothing is found
	return 0;
}

// Working
int oufs_find_bit_positions(unsigned char * byte_array, int * pos, char type) {

	int size = 0;
	if (type == 'I') { // inode table
		size = (N_INODES >> 3);
	} else if (type == 'B') { // block table
		size = (N_BLOCKS_IN_DISK >> 3);
	} else {
		fprintf(stderr, "oufs_find_bit_positions(): Unknown type\n");
		return -1;
	}

	int byte, bit;
	for (int i = 0; i < size; i++) {
		if (oufs_find_available_bit(byte_array[i], &bit)) {
			// Position is found
			byte = i;
			break;
		} else {
			if (i == size - 1) {
				if (type == 'I') fprintf(stderr, "Not enough available inodes to make directory\n");
				if (type == 'B') fprintf(stderr, "Not enough available blocks to make directory\n");
				return -1;
			}
		}
	}

	// Flip bytes in master
	char new_byte = byte_array[byte];
	oufs_flip_bit(&new_byte, bit);
	byte_array[byte] = new_byte;

	*pos = 8 * byte + bit;

	return 0;
}

int oufs_mkdir(char * cwd, char * path){

	INODE_REFERENCE gparent_inode_ref, parent_inode_ref;

	// Search to see if path already exists in cwd
	int find_file = oufs_find_file (cwd, path, &gparent_inode_ref, &parent_inode_ref);

	if (find_file == 0) { // Make directory

		// Get name of directory from path
		char dir_name[FILE_NAME_SIZE];
		char temp[MAX_PATH_LENGTH];
		strcpy(temp, path);
		char * basename_ptr = basename(temp);
		if (strlen(basename_ptr) > FILE_NAME_SIZE) {
			fprintf(stderr, "Directory name too large\n");
			return -1;
		} else {
			strcpy(dir_name, basename_ptr);
		}

		// Read parent inode block
		INODE parent_inode;
		BLOCK_REFERENCE parent_block_ref;

		if (oufs_read_inode_by_reference(parent_inode_ref, &parent_inode) < 0) return -1;

		// Check to see if parent can fit new directory
		if (parent_inode.size == DIRECTORY_ENTRIES_PER_BLOCK) {
			fprintf(stderr, "Not enough space in parent\n");
			return -1;
		}

		// Read master block
		BLOCK block_0;
		if (vdisk_read_block(0, &block_0) < 0) return -1;

		// Get inode/block positions for new inode and new block, modify master block
		int child_inode_ref;
		if (oufs_find_bit_positions(block_0.master.inode_allocated_flag, &child_inode_ref, 'I') < 0) return -1;

		int child_block_ref;
		if (oufs_find_bit_positions(block_0.master.block_allocated_flag, &child_block_ref, 'B') < 0) return -1;

		// Write master block
		if (vdisk_write_block(0, &block_0) < 0) return -1;

		// Modify parent inode block
		parent_inode.size = parent_inode.size + 1;
		parent_block_ref = parent_inode.data[0];

		// Write parent inode block
		if (oufs_write_inode_by_reference(parent_inode_ref, &parent_inode) < 0) return -1;

		// Read parent directory block
		BLOCK parent_dir_block;
		if (vdisk_read_block(parent_block_ref, &parent_dir_block) < 0) return -1;

		// Modify parent directory block
		for (int i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; i++) {
			if (parent_dir_block.directory.entry[i].inode_reference == UNALLOCATED_INODE) {
				strcpy(parent_dir_block.directory.entry[i].name, dir_name);
				parent_dir_block.directory.entry[i].inode_reference = child_inode_ref;
				break;
			}
		}

		// Write parent directory block
		if (vdisk_write_block(parent_block_ref, &parent_dir_block) < 0) return -1;


		// Build new directory block
		BLOCK child_dir_block;

		for (int i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; i++)
			child_dir_block.directory.entry[i].inode_reference = UNALLOCATED_INODE;

		strcpy(child_dir_block.directory.entry[0].name, ".");
		child_dir_block.directory.entry[0].inode_reference = child_inode_ref;

		strcpy(child_dir_block.directory.entry[1].name, "..");
		child_dir_block.directory.entry[1].inode_reference = parent_inode_ref;

		// Write new directory block
		if (vdisk_write_block(child_block_ref, &child_dir_block) < 0) return -1;

		// Make new inode 
		INODE child_inode;

		child_inode.type = IT_DIRECTORY;
		child_inode.n_references = 1;

		for (int i = 0; i < BLOCKS_PER_INODE; i++) 
			child_inode.data[i] = UNALLOCATED_BLOCK;

		child_inode.data[0] = child_block_ref;

		child_inode.size = 2;

		// Write new inode 
		if (oufs_write_inode_by_reference(child_inode_ref, &child_inode) < 0) return -1;

	} else if (find_file == 1) { // Cant make directory, dir name exists

		fprintf(stderr, "Unable to make directory %s, name exists.\n", path);
		return -1;

	} else { // Error

		return -1;

	}

	return 0;
}

int oufs_rmdir(char * cwd, char * path) {

	// Get name of directory from path
	char dir_name[FILE_NAME_SIZE];
	char temp[MAX_PATH_LENGTH];
	strcpy(temp, path);
	char * basename_ptr = basename(temp);
	if (strlen(basename_ptr) > FILE_NAME_SIZE) {
		fprintf(stderr, "Directory name too large\n");
		return -1;
	} else {
		strcpy(dir_name, basename_ptr);
	}

	// Check to make sure name is legal
	if (!strcmp(dir_name, ".") || !strcmp(dir_name, "..")) {
		fprintf(stderr, "Illegal name '%s'\n", dir_name);
		return -1;
	}

	INODE_REFERENCE parent_inode_ref, child_inode_ref;

	int found = oufs_find_file (cwd, path, &parent_inode_ref, &child_inode_ref);
	if (found == 0) {
		fprintf(stderr, "Name does not exist\n");
		return -1;
	} else if (found < 0) {
		// Parent function error
		return -1;
	}

	// Read in child inode and get child dir block from child inode
	INODE child_inode;
	if (oufs_read_inode_by_reference(child_inode_ref, &child_inode) < 0) return -1;
	BLOCK_REFERENCE child_block_ref = child_inode.data[0];

	// Check to make sure child is a directory
	if (child_inode.type != IT_DIRECTORY) {
		fprintf(stderr, "%s is not a directory\n", dir_name);
		return -1;
	}

	// Check to make sure directory is empty
	if (child_inode.size != 2) {
		fprintf(stderr, "%s is not empty\n", dir_name);
		return -1;
	}

	// Read in parent inode and get parent dir block from parent inode
	INODE parent_inode;
	if (oufs_read_inode_by_reference(parent_inode_ref, &parent_inode) < 0) return -1;
	BLOCK_REFERENCE parent_block_ref = parent_inode.data[0];
	
	// Decrease parent inode size by 1
	parent_inode.size--;

	// Write updated parent inode
	if (oufs_write_inode_by_reference(parent_inode_ref, &parent_inode) < 0) return -1;
	
	// Find parent dir entry with dir_name, clear name, set inode_ref to UNALLOCATED_INODE
	BLOCK parent_dir_block;
	if (vdisk_read_block(parent_block_ref, &parent_dir_block) < 0) return -1;
	
	for (int i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; i++) {
		if (!strcmp(dir_name, parent_dir_block.directory.entry[i].name)) {
			memset(parent_dir_block.directory.entry[i].name, 0, sizeof(parent_dir_block.directory.entry[i].name));
			parent_dir_block.directory.entry[i].inode_reference = UNALLOCATED_INODE;
			break;
		}
	}

	// Write updated parent block
	if (vdisk_write_block(parent_block_ref, &parent_dir_block) < 0) return -1;

	// Create empty inode and block, write them to child inode ref and child block ref
	INODE empty_inode;
	BLOCK empty_block;
	memset(&empty_inode, 0, sizeof(empty_inode));
	memset(&empty_block, 0, sizeof(empty_block));
	if (oufs_write_inode_by_reference(child_inode_ref, &empty_inode) < 0) return -1;
	if (vdisk_write_block(child_block_ref, &empty_block) < 0) return -1;

	// Read in master block, update both tables, write back to disk
	BLOCK master_block;
	if (vdisk_read_block(0, &master_block) < 0) return -1;

	int i_byte, i_bit, b_byte, b_bit;
	i_byte = child_inode_ref / 8;
	i_bit = child_inode_ref % 8;
	b_byte = child_block_ref / 8;
	b_bit = child_block_ref % 8;

	char new_i_flag = master_block.master.inode_allocated_flag[i_byte];
	char new_b_flag = master_block.master.block_allocated_flag[b_byte];

	oufs_flip_bit(&new_i_flag, i_bit);
	oufs_flip_bit(&new_b_flag, b_bit);

	master_block.master.inode_allocated_flag[i_byte] = new_i_flag;
	master_block.master.block_allocated_flag[b_byte] = new_b_flag;

	if (vdisk_write_block(0, &master_block) < 0) return -1;

	return 0;
}

int oufs_find_open_bit(unsigned char value){

	return 0;
}

/*
int oufs_comparator(const void *a, const void *b) {
	const char * aa = *(const char **) a;
	const char * bb = *(const char **) b;
	return strncmp(aa, bb, sizeof(char *));
}
*/

int oufs_comparator(const void *a, const void *b) {
	DIRECTORY_ENTRY const * aa = (DIRECTORY_ENTRY const *) a;
	DIRECTORY_ENTRY const * bb = (DIRECTORY_ENTRY const *) b;
	return strncmp(aa->name, bb->name, sizeof(char *));
}

// TODO: STILL BROKEN
int oufs_list(char * cwd, char * path) {

	// If no path is given, use cwd as path
	if (!path) path = ".";

	// Locate inode of path
	INODE_REFERENCE parent_inode_ref, inode_ref;
	if (oufs_find_file (cwd, path, &parent_inode_ref, &inode_ref) < 1) {
		fprintf(stderr, "File does not exist\n");
		return -1;
	}

	// Load in inode of path
	INODE inode;
	memset(&inode, 0, sizeof(inode));
	if (oufs_read_inode_by_reference(inode_ref, &inode) < 0) return -1;		

	// Check to see if inode is a file
	if (inode.type == IT_FILE) {
		fprintf(stdout, "%s\n", path);
		return 0;
	} else if (inode.type == IT_NONE) {
		return -1;
	}

	// Load directory block into memory
	BLOCK dir_block;
	memset(&dir_block, 0, sizeof(dir_block));
	if (vdisk_read_block(inode.data[0], &dir_block) < 0) return -1;

	// Sort dir_block by name
	qsort(dir_block.directory.entry, DIRECTORY_ENTRIES_PER_BLOCK, sizeof(*dir_block.directory.entry), oufs_comparator);

	// Loop through directory entries in loaded block
	INODE temp_inode;
	for (int i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; i++) {

		// Loop through all allocated entries in directory
		if (dir_block.directory.entry[i].inode_reference != UNALLOCATED_INODE) {
			// Clear temp inode
			memset(&inode, 0, sizeof(inode));

			// Read inode reference in directory
			oufs_read_inode_by_reference(dir_block.directory.entry[i].inode_reference, &temp_inode);

			// Print first part of entry
			fprintf(stdout, "%s", dir_block.directory.entry[i].name);

			// Check to see if entry is a directory, append '/' if so
			if (temp_inode.type == IT_DIRECTORY)
				fputs("/", stdout);

			// Finish with newline
			fputs("\n", stdout);
		}
	}

	return 0;
}
