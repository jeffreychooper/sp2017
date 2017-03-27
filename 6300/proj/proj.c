#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

void ErrorCheck(int val, char *str);
int GetInfoFromFiles(FILE *mapFile, FILE *networkGraphFile, FILE *taskGraphFile);

// beyond the info found in the user provided files we need:
	// array of links with the number of files traveling over each link currently
	// array of nodes with the number of modules being executed currently
	// transfer array... based partially on provided info
	// execution array... based partially on provided info

int main(int argc, char *argv[])
{
	// get the map file, network graph, and task graph from the command line
	if(argc < 4)
	{
		puts("Usage: proj mapFile, networkGraph, taskGraph");
		return 1;
	}

	// open the files
	FILE *mapFile = fopen(argv[1], "r");
	FILE *networkGraphFile = fopen(argv[2], "r");
	FILE *taskGraphFile = fopen(argv[3], "r");

	// get relevant information from each file
	if(GetInfoFromFiles(mapFile, networkGraphFile, taskGraphFile) != 0)
	{
		puts("Error in setup files");
		return 1;
	}

	fclose(mapFile);
	fclose(networkGraphFile);
	fclose(taskGraphFile);

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

int GetInfoFromFiles(FILE *mapFile, FILE *networkGraphFile, FILE *taskGraphFile)
{
	// number, ids, and node mappings of modules (map file)

	// computational requirements of modules (task graph)

	// number of dependencies... which modules a node is dependent upon and how much data it needs from the other module (task graph)

	// node processing power (network graph)

	// relevant link bandwidths/static delays... look at dependencies/module to node mappings to decide which links we care about (network graph)

	return 0;
}
