// TODO: All sorts of efficiency issues... Triple loops everywhere :|
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <float.h>

#define MAX_LINE_LENGTH 32

typedef struct IDListInfo IDListInfo;

struct IDListInfo
{
	int id;
	IDListInfo *prevInfoPointer;
	IDListInfo *nextInfoPointer;
};

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
	int numUsing;
	IDListInfo *firstUsing;
	IDListInfo *lastUsing;
} NodeInfo;

typedef struct
{
	int id;
	int node1;
	int node2;
	int numUsing;
	IDListInfo *firstUsing;
	IDListInfo *lastUsing;
} LinkInfo;

typedef struct
{
	int linkID;				// set
	int module1;			// set
	int module2;			// set
	double startTime;
	double endTime;
	double remainingData;	// set
	double remainingDelay;	// set
} TransferInfo;

typedef struct
{
	int moduleID;					// set
	IDListInfo *firstDependency;	// set (possibly null)
	IDListInfo *lastDependency;		// set (possibly null)
	int *dependencyMet;				// set (possibly null)
	double startTime;
	double endTime;
	double remainingComputation;	// set
} ExecutionInfo;

// variables found in the user provided files
int numModules;
ModuleInfo *moduleInfo;
int numNodes;
NodeInfo *nodeInfo;

// variables for keeping track of state
int numLinksUsed;
int numNodesUsed;
int numDependencies;
LinkInfo *linksUsed;
NodeInfo *nodesUsed;
TransferInfo *transferInfo;
ExecutionInfo *executionInfo;

void ErrorCheck(int val, char *str);
int GetInfoFromFiles(FILE *mapFile, FILE *networkGraphFile, FILE *taskGraphFile);
void PrepareStateVariables();
void CalculateTimeRequirements();
IDListInfo *AddIDInfo(int id, IDListInfo *first, IDListInfo *last);
void RemoveIDInfo(IDListInfo *info, IDListInfo *first, IDListInfo *last);

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

	PrepareStateVariables();

	CalculateTimeRequirements();

	// clear any remaining memory

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

void PrepareStateVariables()
{
	// count the number of dependencies and unique nodes
	int foundNodes[numModules];

	for(int moduleIndex = 0; moduleIndex < numModules; moduleIndex++)
	{
		numDependencies += moduleInfo[moduleIndex].numDependents;

		int found = 0;

		for(int nodeIndex = 0; nodeIndex < numNodesUsed; nodeIndex++)
			if(moduleInfo[moduleIndex].node == foundNodes[nodeIndex])
				found = 1;

		if(!found)
		{
			foundNodes[numNodesUsed] = moduleInfo[moduleIndex].node;
			numNodesUsed++;
		}
	}

	// store the unique nodes
	nodesUsed = malloc(sizeof(NodeInfo) * numNodesUsed);			// TODO: free

	for(int i = 0; i < numNodesUsed; i++)
	{
		int id = foundNodes[i];
		nodesUsed[i].id = id;
		nodesUsed[i].processingPower = nodeInfo[id].processingPower;
		nodesUsed[i].numUsing = 0;
		nodesUsed[i].firstUsing = NULL;
		nodesUsed[i].lastUsing = NULL;
	}

	// find all of the unique links used
	int foundLinks[numDependencies][2];

	for(int moduleIndex = 0; moduleIndex < numModules; moduleIndex++)
	{
		ModuleInfo currModule = moduleInfo[moduleIndex];

		for(int dependentIndex = 0; dependentIndex < currModule.numDependents; dependentIndex++)
		{
			int found = 0;

			for(int linkIndex = 0; linkIndex < numLinksUsed; linkIndex++)
				if(foundLinks[linkIndex][0] == currModule.node && foundLinks[linkIndex][1] == currModule.dependentNodes[dependentIndex])
					found = 1;

			if(!found)
			{
				foundLinks[numLinksUsed][0] = currModule.node;
				foundLinks[numLinksUsed][1] = currModule.dependentNodes[dependentIndex];
				numLinksUsed++;
			}
		}
	}

	// store the unique links
	linksUsed = malloc(sizeof(LinkInfo) * numLinksUsed);			// TODO: free

	for(int i = 0; i < numLinksUsed; i++)
	{
		linksUsed[i].id = i;
		linksUsed[i].node1 = foundLinks[i][0];
		linksUsed[i].node2 = foundLinks[i][1];
		linksUsed[i].numUsing = 0;
		linksUsed[i].firstUsing = NULL;
		linksUsed[i].lastUsing = NULL;
	}

	// store the known transfer info
	transferInfo = malloc(sizeof(TransferInfo) * numDependencies);			// TODO: free

	int transferIndex = 0;

	for(int moduleIndex = 0; moduleIndex < numModules; moduleIndex++)
	{
		ModuleInfo currModule = moduleInfo[moduleIndex];

		for(int dependentIndex = 0; dependentIndex < currModule.numDependents; dependentIndex++)
		{
			for(int linkIndex = 0; linkIndex < numLinksUsed; linkIndex++)
				if(foundLinks[linkIndex][0] == currModule.node && foundLinks[linkIndex][1] == currModule.dependentNodes[dependentIndex])
					transferInfo[transferIndex].linkID = linkIndex;

			transferInfo[transferIndex].module1 = currModule.id;
			transferInfo[transferIndex].module2 = currModule.dependentModules[dependentIndex];
			transferInfo[transferIndex].remainingData = currModule.dataToProvide[dependentIndex];
			transferInfo[transferIndex].remainingDelay = currModule.dependentDelays[dependentIndex];
			transferInfo[transferIndex].startTime = DBL_MAX;
			transferInfo[transferIndex].endTime = DBL_MAX;

			transferIndex++;
		}
	}

	// store the known execution info
	executionInfo = malloc(sizeof(ExecutionInfo) * numModules);			// TODO: free

	for(int moduleIndex = 0; moduleIndex < numModules; moduleIndex++)
	{
		ModuleInfo currModule = moduleInfo[moduleIndex];

		executionInfo[moduleIndex].moduleID = moduleIndex;

		if(currModule.numProviders == 1)
		{
			// currModule.providerModules[0]
			executionInfo[moduleIndex].firstDependency = AddIDInfo(currModule.providerModules[0], NULL, NULL);
			executionInfo[moduleIndex].lastDependency = executionInfo[moduleIndex].firstDependency;
			
			executionInfo[moduleIndex].dependencyMet = malloc(sizeof(int) * currModule.numProviders);
			executionInfo[moduleIndex].dependencyMet[0] = 0;
		}
		else if(currModule.numProviders > 0)
		{
			executionInfo[moduleIndex].firstDependency = AddIDInfo(currModule.providerModules[0], NULL, NULL);
			executionInfo[moduleIndex].lastDependency = executionInfo[moduleIndex].firstDependency;

			for(int i = 1; i < currModule.numProviders; i++)
			{
				executionInfo[moduleIndex].lastDependency = AddIDInfo(currModule.providerModules[i], executionInfo[moduleIndex].firstDependency, executionInfo[moduleIndex].lastDependency);
			}

			executionInfo[moduleIndex].dependencyMet = malloc(sizeof(int) * currModule.numProviders);

			for(int i = 0; i < currModule.numProviders; i++)
			{
				executionInfo[moduleIndex].dependencyMet[i] = 0;
			}
		}
		else
		{
			executionInfo[moduleIndex].firstDependency = NULL;
			executionInfo[moduleIndex].lastDependency = NULL;
			executionInfo[moduleIndex].dependencyMet = NULL;
		}

		executionInfo[moduleIndex].remainingComputation = currModule.computationalComplexity;
		executionInfo[moduleIndex].startTime = DBL_MAX;
		executionInfo[moduleIndex].endTime = DBL_MAX;
	}
}

// TODO: Not doing multiple runs together right now...
void CalculateTimeRequirements()
{
	// !!!!!!!!!!!!!!!!!!!REMEMBER YOU FUCKING IDIOT!!!!!!!!!!!!!!!!!!!!!
	// delay is only added once to a transfer... at the beginning
	// BUT IT ISN'T ON THE LINK WHEN THE DELAY IS HAPPENING DUMBASS
	// should check when a new transfer would begin if it needs to actually move nodes... if not it's instant

	double currentTime = 0.0;
	int done = 0;
	
	// module 0 instantly begins transferring data to its dependents
	// prepare the data structures for tracking the links it will use
	executionInfo[0].startTime = currentTime;
	executionInfo[0].endTime = currentTime;

	for(int transferIndex = 0; transferIndex < numDependencies; transferIndex++)
	{
		TransferInfo currTransfer = transferInfo[transferIndex];
		if(currTransfer.module1 == 0)
		{
			linksUsed[currTransfer.linkID].numUsing++;

			linksUsed[currTransfer.linkID].lastUsing = AddIDInfo(transferIndex, linksUsed[currTransfer.linkID].firstUsing, linksUsed[currTransfer.linkID].lastUsing);

			if(linksUsed[currTransfer.linkID].firstUsing == NULL)
				linksUsed[currTransfer.linkID].firstUsing = linksUsed[currTransfer.linkID].lastUsing;
		}
	}
	
	while(!done)
	{
		// NOTE: HAVE TO DO TRANSFERS AND EXECUTIONS AT SAME TIME
		double shortestTime = 9999999.9;
		int shortestIsTransfer = 0;
		int shortestIndex = -1;

		// find and store the shortest time that is needed
		for(int executionIndex = 0; executionIndex < numModules; executionIndex++)
		{
			
		}

		// place the time value for the shortest transfer or execution in the array to signal its completion
		// check if the completion of the transfer/execution allows something else to start
		// remove the finished one from its link/node
		// calculate the amount of data or compuation time left for each transfer or execution
		// advance time
		// check that at least one node or link has work to do
	}
}

IDListInfo *AddIDInfo(int id, IDListInfo *first, IDListInfo *last)
{
	IDListInfo *newInfo = malloc(sizeof(IDListInfo));

	newInfo->id = id;
	
	if(first == NULL)
	{
		first = newInfo;
		newInfo->prevInfoPointer = NULL;
		newInfo->nextInfoPointer = NULL;
	}
	else
	{
		last->nextInfoPointer = newInfo;
		newInfo->prevInfoPointer = last;
		newInfo->nextInfoPointer = NULL;
	}

	last = newInfo;

	return newInfo;
}

void RemoveIDInfo(IDListInfo *info, IDListInfo *first, IDListInfo *last)
{
	if(info == first)
	{
		first = NULL;
		last = NULL;
	}
	else if(info == last)
	{
		last = info->prevInfoPointer;
		last->nextInfoPointer = NULL;
	}
	else
	{
		info->prevInfoPointer->nextInfoPointer = info->nextInfoPointer;
		info->nextInfoPointer->prevInfoPointer = info->prevInfoPointer;
	}

	free(info);
}
