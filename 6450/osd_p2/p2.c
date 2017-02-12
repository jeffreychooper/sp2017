#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_LINE_LENGTH 896
#define CONFIG_MAX_SIZE 42
#define OPERATIONS_START_ELEMENTS 20
#define MAX_LINE_TOKENS 14
#define MAX_TOKEN_LENGTH 64
#define EXTRA_OPERATIONS 10

enum NodeType { SWITCH, ROUTER, HOST };

// switch information
typedef struct {

unsigned char netNumber;
unsigned char knownMACs[6];
int interfaces[6];

} SwitchInfo;

// router information
typedef struct {

unsigned char MACs[6];
unsigned char netIPs[6];
unsigned char hostIPs[6];
int interfaces[6];
int interpreterFD;

} RouterInfo;

// host information
typedef struct {

unsigned char MAC;
unsigned char netIP;
unsigned char hostIP;
int inferface;
int interpreterFD;

} HostInfo;

void ActAsSwitch();
void ActAsRouter();
void ActAsHost();

int main(int argc, char *argv[])
{
	// interpret the file given by the user
	if(argc < 2)
	{
		printf("Usage: ./p2 filename\n");
		return 1;
	}

	char *filename = argv[1];

	FILE *userFile = fopen(filename, "r");

	char line[MAX_LINE_LENGTH];		// hold the lines as they're read from the file
	char *p;						// general use pointer

	char config[CONFIG_MAX_SIZE][MAX_LINE_TOKENS][MAX_TOKEN_LENGTH];				// the configuration specified in the file

	// TODO: free
	char ***operations = malloc(OPERATIONS_START_ELEMENTS * sizeof(char**));		// the operations specified in the file

	for(int i = 0; i < OPERATIONS_START_ELEMENTS; i++)
	{
		// TODO: free
		operations[i] = malloc(MAX_LINE_TOKENS * sizeof(char *));

		for(int j = 0; j < MAX_LINE_TOKENS; j++)
		{
			// TODO: free
			operations[i][j] = calloc(MAX_TOKEN_LENGTH, sizeof(char));
		}
	}

	int configDone = 0;
	int lineCount = 0;
	int tokenIndex = 0;
	int currMaxOperations = OPERATIONS_START_ELEMENTS;
	int numConfig = 0;
	int numOperations = 0;

	while(fgets(line, MAX_LINE_LENGTH, userFile) != NULL)
	{
		line[strlen(line) - 1] = '\0';

		if(p = (strchr(line, '#')))
			*p = '\0';

		if(!configDone && line[0] != 'v')
		{
			configDone = 1; 
			lineCount = 0;
		}


		if(!configDone)
		{
			if(p = strtok(line, " \t"))
			{
				strcpy(config[lineCount][0], p);

				for(tokenIndex = 1; tokenIndex < MAX_LINE_TOKENS; tokenIndex++)
				{
					if(p = strtok(NULL, " \t"))
						strcpy(config[lineCount][tokenIndex], p);
					else
						break;
				}

				numConfig++;
				lineCount++;
			}
		}
		else
		{
			if(lineCount >= currMaxOperations)
			{
				int prevMaxOperations = currMaxOperations;
				currMaxOperations += EXTRA_OPERATIONS;

				// TODO: free
				operations = realloc(operations, currMaxOperations * sizeof(char**));

				for(int i = prevMaxOperations; i < currMaxOperations; i++)
				{
					// TODO: free
					operations[i] = malloc(MAX_LINE_TOKENS * sizeof(char *));

					for(int j = 0; j < MAX_LINE_TOKENS; j++)
					{
						// TODO: free
						operations[i][j] = calloc(MAX_TOKEN_LENGTH, sizeof(char));
					}
				}
			}

			if(p = strtok(line, " \t"))
			{
				strcpy(operations[lineCount][0], p);

				for(tokenIndex = 1; tokenIndex < MAX_LINE_TOKENS; tokenIndex++)
				{
					if(p = strtok(NULL, " \t"))
						strcpy(operations[lineCount][tokenIndex], p);
					else
						break;
				}

				numOperations++;
				lineCount++;
			}
		}

	}

	fclose(userFile);

	SwitchInfo switches[6];
	RouterInfo routers[36];
	HostInfo hosts[36];
	int numSwitches = 0;
	int numRouters = 0;
	int numHosts = 0;

	for(int i = 0; i < numConfig; i++)
	{
		switch(config[i][0][1])
		{
			case 's':
				switches[numSwitches].netNumber = (unsigned char)atoi(config[i][2]);
				numSwitches++;
				break;
			case 'r':
				for(int j = 2; config[i][j][0] != 0; j += 2)
				{
					routers[numRouters].MACs[(j - 2) / 2] = (unsigned char)atoi(config[i][j]);
					
					if(p = strtok(config[i][j + 1], "."))
					{
						routers[numRouters].netIPs[(j - 2) / 2] = (unsigned char)atoi(p);

						if(p = strtok(NULL, " \0"))
							routers[numRouters].hostIPs[(j - 2) / 2] = (unsigned char)atoi(p);
					}
				}
				numRouters++;
				break;
			case 'h':
				hosts[numHosts].MAC = (unsigned char)atoi(config[i][2]);

				if(p = strtok(config[i][3], "."))
				{
					hosts[numHosts].netIP = (unsigned char)atoi(p);

					if(p = strtok(NULL, " \0"))
						hosts[numHosts].hostIP = (unsigned char)atoi(p);
				}
				numHosts++;
				break;
		}
	}

	// add switch known macs
	for(int i = 0; i < numSwitches; i++)
	{
		int knownMACs = 0;
		for(int j = 0; j < numRouters; j++)
		{
			for(int k = 0; routers[j].netIPs[k] != 0; k++)
			{
				if(routers[j].netIPs[k] == switches[i].netNumber)
				{
					switches[i].knownMACs[knownMACs] = routers[j].MACs[k];
					knownMACs++;
				}
			}
		}

		for(int j = 0; j < numHosts; j++)
		{
			if(hosts[j].netIP == switches[i].netNumber)
			{
				switches[i].knownMACs[knownMACs] = hosts[j].MAC;
				knownMACs++;
			}
		}
	}

	return 0;
}
