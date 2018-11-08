#include "oufs_lib.h"

int main (int argc, char * argv[]){

	char cwd[MAX_PATH_LENGTH];
	char virtual_disk_name[MAX_PATH_LENGTH];

	oufs_get_environment(cwd, virtual_disk_name);

	oufs_format_disk(virtual_disk_name);

	/*
	 *  128 blocks of 256 bytes each
	 *  block 0 is 256 bytes
	 */

	// MASTER_BLOCK master_block;

	fprintf(stdout, "%d\n", BLOCK_SIZE);
	fprintf(stdout, "%ld\n", sizeof(DIRECTORY_ENTRY));
	// fprintf(stdout, "%ld\n", );

	return 0;
}
