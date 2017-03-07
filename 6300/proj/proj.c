void ErrorCheck(int val, char *str);

int main(int argc, char *argv[])
{
	// get the map file, network graph, and task graph from the command line
	if(argc < 

	// open the files

	// get relevant information from each file

	return 0;
}

void ErrorCheck(int val, char *str)
{
	if(val < 0)
	{
		printf("%s : %d : %s\n", str, val, strerror(errno));
		exit(1);
	}
}
