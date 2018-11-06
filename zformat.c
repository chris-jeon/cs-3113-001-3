#include "oufs_lib.h"

int main (int argc, char * argv[]){

	char * one = "jackson";
	char * two = "taylor";
	oufs_rmdir(one, two);

	return 0;
}
