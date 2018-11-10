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
		if (parent_inode.size == BLOCKS_PER_INODE) {
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
		for (int i = 0; i < BLOCKS_PER_INODE; i++) {
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


	/*

	// If there is no file found, make directory
	int found = oufs_find_file (cwd, path, &grandparent, &parent_inode_no);
	if (found == 0){

		// Get name from path
		char temp[MAX_PATH_LENGTH];
		strcpy(temp, path);
		char file_name[FILE_NAME_SIZE];
		strcpy(file_name, basename(temp));

		// Get parent inode
		INODE parent_inode;
		oufs_read_inode_by_reference(parent_inode_no, &parent_inode);

		// Check to see if the parent data block has space
		if (parent_inode.size >= DIRECTORY_ENTRIES_PER_BLOCK) {
			fprintf(stderr, "Unable to create directory. Parent too full.\n");
			return -1;
		}

		BLOCK master_block;
		vdisk_read_block(0, &master_block);

		// Find inode position for child
		int child_inode_no;
		oufs_find_bit_positions(master_block.master.inode_allocated_flag, &child_inode_no, 'I');

		// Find block position for child
		int child_block_no;
		oufs_find_bit_positions(master_block.master.block_allocated_flag, &child_block_no, 'B');

		// Update master block
		vdisk_write_block(0, &master_block);

		// Build child directory block
		BLOCK child_block;
		strcpy(child_block.directory.entry[0].name, ".");
		child_block.directory.entry[0].inode_reference = child_inode_no;
		strcpy(child_block.directory.entry[1].name, "..");
		child_block.directory.entry[1].inode_reference = parent_inode_no;
		for (int i = 2; i < DIRECTORY_ENTRIES_PER_BLOCK; i++)
			child_block.directory.entry[i].inode_reference = UNALLOCATED_INODE;

		// Write child block
		vdisk_write_block(child_block_no, &child_block);

		// Build child inode
		INODE child_inode;
		child_inode.type = IT_DIRECTORY;
		child_inode.n_references = 1;
		for (int i = 0; i < BLOCKS_PER_INODE; i++) {
			if (i == 0)
				child_inode.data[i] = child_block_no;
			else
				child_inode.data[i] = UNALLOCATED_BLOCK;
		}
		child_inode.size = 2;

		// Write child inode
		oufs_write_inode_by_reference(child_inode_no, &child_inode);

		// Rebuild parent block
		BLOCK parent_block;
		vdisk_read_block(parent_inode.data[0], &parent_block);
		for (int i = 0; i < DIRECTORY_ENTRIES_PER_BLOCK; i++){
			if (parent_block.directory.entry[i].inode_reference == UNALLOCATED_INODE) {
				strcpy(parent_block.directory.entry[i].name, file_name);
				parent_block.directory.entry[i].inode_reference = child_inode_no;
				break;
			}
		}

		// Write parent block
		vdisk_write_block(parent_inode.data[0], &parent_block);

		// Updade parent inode size
		parent_inode.size++;

		// Write parent inode
		oufs_write_inode_by_reference(parent_inode_no, &parent_inode);

	} else if (found == 1) {

		fprintf(stderr, "%s already exists in %s\n", path, cwd);
		return -1;
	} else {

		fprintf(stderr, "Error walking path\n");
		return -1;
	}
	*/

	return 0;
}

int oufs_list(char * cwd, char * path){

	return 0;
}

int oufs_rmdir(char * cwd, char * path){

	return 0;
}

int oufs_find_open_bit(unsigned char value){

	return 0;
}
