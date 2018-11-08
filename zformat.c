#include "oufs_lib.h"

int main (int argc, char * argv[]){

	char cwd[MAX_PATH_LENGTH];
	char virtual_disk_name[MAX_PATH_LENGTH];
	oufs_get_environment(cwd, virtual_disk_name);
	oufs_format_disk(virtual_disk_name);

	return 0;
}
