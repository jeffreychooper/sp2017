#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define MAX_LINE_LENGTH 32

void ErrorCheck(int val, char *str);
int GetInfoFromFiles(FILE *mapFile, FILE *networkGraphFile, FILE *taskGraphFile);
void CalculateTimeRequirements();

typedef struct
{
	int id;
	int node;
	double computationalComplexity;

	int numDependents;
	int *dependentModules;
	double *dataToProvide;
	int *dependentNodes;
	double *dependentBandwidths;
	double *dependentDelays;

	int numProviders;
	int *providerModules;
	double *providedData;
	int *providerNodes;
	double *providerBandwidths;
	double *providerDelays;
} ModuleInfo;

typedef struct
{
	int id;
	double processingPower;
} NodeInfo;

// variables found in the user provided files
int numModules;
ModuleInfo *moduleInfo;
int numNodes;
NodeInfo *nodeInfo;

// beyond the info found in the user provided files we need:
	// array of links with the number of files traveling over each link currently and an "in use" flag
	// array of nodes with the number of modules being executed currently and an "in use" flag
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

	// close the files
	fclose(mapFile);
	fclose(networkGraphFile);
	fclose(taskGraphFile);

	// Prepare Transfer and Execution Arrays?

	CalculateTimeRequirements();

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
	char *lineBuffer;
	lineBuffer = malloc(sizeof(char) * MAX_LINE_LENGTH);
	char *tokenBuffer;

	// number, ids, and node mappings of modules (map file)
	fgets(lineBuffer, 32, mapFile);
	strtok(lineBuffer, " ");
	tokenBuffer = strtok(NULL, "\n");
	numModules = atoi(tokenBuffer);

	moduleInfo = malloc(sizeof(ModuleInfo) * numModules);			// TODO: free

	for(int moduleIndex = 0; moduleIndex < numModules; moduleIndex++)
	{
		fgets(lineBuffer, 32, mapFile);
		tokenBuffer = strtok(lineBuffer, " ");
		moduleInfo[moduleIndex].id = atoi(tokenBuffer);
		tokenBuffer = strtok(NULL, "\n");
		moduleInfo[moduleIndex].node = atoi(tokenBuffer);
	}

	// computational requirements of modules (task graph)
	fgets(lineBuffer, 32, taskGraphFile);

	for(int moduleIndex = 0; moduleIndex < numModules; moduleIndex++)
	{
		fgets(lineBuffer, 32, taskGraphFile);
		strtok(lineBuffer, " ");
		tokenBuffer = strtok(NULL, "\n");
		moduleInfo[moduleIndex].computationalComplexity = strtod(tokenBuffer, NULL);
	}

	// number of dependencies... which modules a node is dependent upon and how much data it needs from the other module (task graph)
	fgets(lineBuffer, 32, taskGraphFile);

	int provider;
	int dependent;
	int nodeDependentCount[numModules];
	int nodeProviderCount[numModules];
	double dependencyArray[numModules][numModules];			// provider, dependent

	for(int i = 0; i < numModules; i++)
	{
		nodeDependentCount[i] = 0;
		nodeProviderCount[i] = 0;
	}

	for(int i = 0; i < numModules * numModules; i++)
	{
		fgets(lineBuffer, 32, taskGraphFile);
		tokenBuffer = strtok(lineBuffer, " ");
		provider = atoi(tokenBuffer);
		tokenBuffer = strtok(NULL, " ");
		dependent = atoi(tokenBuffer);
		tokenBuffer = strtok(NULL, "\n");
		dependencyArray[provider][dependent] = strtod(tokenBuffer, NULL);
		if(dependencyArray[provider][dependent] > 0)
		{
			nodeDependentCount[provider]++;
			nodeProviderCount[dependent]++;
		}
	}


	// OH MY GOD WHAT I'M ABOUT TO DO IS INEFFICIENT AS HELL
	for(int moduleIndex = 0; moduleIndex < numModules; moduleIndex++)
	{
		// TODO: free all of this shit (FOR ALL MODULES YOU DUMBASS)
		moduleInfo[moduleIndex].numDependents = nodeDependentCount[moduleIndex];
		moduleInfo[moduleIndex].dependentModules = malloc(sizeof(int) * nodeDependentCount[moduleIndex]);
		moduleInfo[moduleIndex].dataToProvide = malloc(sizeof(double) * nodeDependentCount[moduleIndex]);
		moduleInfo[moduleIndex].dependentNodes = malloc(sizeof(int) * nodeDependentCount[moduleIndex]);
		moduleInfo[moduleIndex].dependentBandwidths = malloc(sizeof(double) * nodeDependentCount[moduleIndex]);
		moduleInfo[moduleIndex].dependentDelays = malloc(sizeof(double) * nodeDependentCount[moduleIndex]);
		
		moduleInfo[moduleIndex].numProviders = nodeProviderCount[moduleIndex];
		moduleInfo[moduleIndex].providerModules = malloc(sizeof(int) * nodeProviderCount[moduleIndex]);
		moduleInfo[moduleIndex].providedData = malloc(sizeof(double) * nodeProviderCount[moduleIndex]);
		moduleInfo[moduleIndex].providerNodes = malloc(sizeof(int) * nodeProviderCount[moduleIndex]);
		moduleInfo[moduleIndex].providerBandwidths = malloc(sizeof(double) * nodeProviderCount[moduleIndex]);
		moduleInfo[moduleIndex].providerDelays = malloc(sizeof(double) * nodeProviderCount[moduleIndex]);

		int dependentIndex = 0;

		for(int i = 0; i < nodeDependentCount[moduleIndex]; i++)
		{
			while(dependencyArray[moduleIndex][dependentIndex] == 0)
				dependentIndex++;

			moduleInfo[moduleIndex].dependentModules[i] = dependentIndex;
			moduleInfo[moduleIndex].dataToProvide[i] = dependencyArray[moduleIndex][dependentIndex];
			moduleInfo[moduleIndex].dependentNodes[i] = moduleInfo[dependentIndex].node;
			dependentIndex++;
		}

		int providerIndex = 0;

		for(int i = 0; i < nodeProviderCount[moduleIndex]; i++)
		{
			while(dependencyArray[providerIndex][moduleIndex] == 0)
				providerIndex++;

			moduleInfo[moduleIndex].providerModules[i] = providerIndex;
			moduleInfo[moduleIndex].providedData[i] = dependencyArray[providerIndex][moduleIndex];
			moduleInfo[moduleIndex].providerNodes[i] = moduleInfo[providerIndex].node;
			providerIndex++;
		}
	}

	// node processing power (network graph)
	fgets(lineBuffer, 32, networkGraphFile);
	strtok(lineBuffer, " ");
	tokenBuffer = strtok(NULL, "\n");
	int numNodes = atoi(tokenBuffer);

	nodeInfo = malloc(sizeof(NodeInfo) * numNodes);			// TODO: free

	for(int nodeIndex = 0; nodeIndex < numNodes; nodeIndex++)
	{
		fgets(lineBuffer, 32, networkGraphFile);
		tokenBuffer = strtok(lineBuffer, " ");
		nodeInfo[nodeIndex].id = atoi(tokenBuffer);
		tokenBuffer = strtok(NULL, "\n");
		nodeInfo[nodeIndex].processingPower = strtod(tokenBuffer, NULL);
	}

	// link bandwidths/delays (network graph)
	fgets(lineBuffer, 32, networkGraphFile);

	int adjacencyIndex;
	int node1;
	int node2;
	double adjacencyBandwidth[numNodes][numNodes];
	double adjacencyDelay[numNodes][numNodes];

	for(int i = 0; i < numNodes * numNodes; i++)
	{
		fgets(lineBuffer, 32, networkGraphFile);
		tokenBuffer = strtok(lineBuffer, " ");
		adjacencyIndex = atoi(tokenBuffer);
		node1 = adjacencyIndex / numNodes;
		node2 = adjacencyIndex % numNodes;
		tokenBuffer = strtok(NULL, " ");
		adjacencyBandwidth[node1][node2] = strtod(tokenBuffer, NULL);
		tokenBuffer = strtok(NULL, "\n");
		adjacencyDelay[node1][node2] = strtod(tokenBuffer, NULL);
	}

	for(int moduleIndex = 0; moduleIndex < numModules; moduleIndex++)
	{
		for(int dependentIndex = 0; dependentIndex < moduleInfo[moduleIndex].numDependents; dependentIndex++)
		{
			int moduleNode = moduleInfo[moduleIndex].node;
			int dependentID = moduleInfo[moduleIndex].dependentModules[dependentIndex];
			int dependentNode = moduleInfo[dependentID].node;

			moduleInfo[moduleIndex].dependentBandwidths[dependentIndex] = adjacencyBandwidth[moduleNode][dependentNode];
			moduleInfo[moduleIndex].dependentDelays[dependentIndex] = adjacencyDelay[moduleNode][dependentNode];
		}

		for(int providerIndex = 0; providerIndex < moduleInfo[moduleIndex].numProviders; providerIndex++)
		{
			int moduleNode = moduleInfo[moduleIndex].node;
			int providerID = moduleInfo[moduleIndex].providerModules[providerIndex];
			int providerNode = moduleInfo[providerID].node;

			moduleInfo[moduleIndex].providerBandwidths[providerIndex] = adjacencyBandwidth[providerNode][moduleNode];
			moduleInfo[moduleIndex].providerDelays[providerIndex] = adjacencyDelay[providerNode][moduleNode];
		}
	}

	free(lineBuffer);

	return 0;
}

void CalculateTimeRequirements()
{
	// time = 0.0
	// done = 0

	// begin on module 0, which immediately begins transferring data to its dependents
	// mark the needed links as in use
	// increment the number in the currently used links/nodes and build the list of indices in the tables
	
	// while not done
		// NOTE: HAVE TO DO TRANSFERS AND EXECUTIONS AT SAME TIME
		// shortestTime = infinite?
		// find and store the shortest time that is needed
		// place the time value for the shortest transfer or execution in the array to signal its completion
		// remove the finished one from its link/node
		// calculate the amount of data or compuation time left for each transfer or execution
		// advance time
		// check that at least one node or link has work to do
}
