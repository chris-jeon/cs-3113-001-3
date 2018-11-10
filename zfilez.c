
int main (int argc, char * argv[]){

	// If no parameter is passed to zfilez you should print all the files in the ZCWD.
	// zfilez: while we have suggested using qsort() in the directory block to generate an ASCII-order listing, do not write this block back to the virtual disk.

	// No name is specified: list the current working directory
	if (!argv[1])
		fprintf(stdout, "No arg given\n");	

	// A directory is specified: list the contents of the directory
	
	// A file is specified: give the name of a file
	
	// It is an error if name does not exist in the file system
	
	return 0;
}
