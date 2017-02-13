// SOCKPAIR SIDES
// 0 interpreter-host/router 1
// 0 switch-router/host 1
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

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

void FreeOperationsMemory(char ***operations, int currMaxOperations);
void ActAsSwitch(SwitchInfo *switchInfo);
void ActAsRouter(RouterInfo *routerInfo);
void ActAsHost(HostInfo *hostInfo);

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

	SwitchInfo switches[6] = {{ 0 }};
	RouterInfo routers[36] = {{ 0 }};
	HostInfo hosts[36] = {{ 0 }};
	int numSwitches = 0;
	int numRouters = 0;
	int numHosts = 0;
	// TODO: close all of these
	int routerControlFDs[36];
	int hostControlFDs[36];

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

	int routerIndex;
	int routerInterfaceIndex;
	int switchIndex;
	int switchInterfaceIndex;
	int hostIndex;

	// go through each switch and make all the sockpairs

	// for each router
	for(routerIndex = 0; routerIndex < numRouters; routerIndex++)
	{
		routerInterfaceIndex = 0;

		// for each router interface (switch)
		while(routers[routerIndex].netIPs[routerInterfaceIndex] != 0)
		{
			switchIndex = 0;

			while(switches[switchIndex].netNumber != routers[routerIndex].netIPs[routerInterfaceIndex])
				switchIndex++;

			switchInterfaceIndex = 0;

			// for each switch interface (host)
			while(switches[switchIndex].knownMACs[switchInterfaceIndex] != 0)
			{
				// if that interface hasn't been configured
				if(switches[switchIndex].interfaces[switchInterfaceIndex] == 0)
				{
					hostIndex = 0;

					while(hosts[hostIndex].MAC != switches[switchIndex].knownMACs[switchInterfaceIndex])
						hostIndex++;

					int ihSockpair[2]; 				// interpreter-host sockpair
					int shSockpair[2];				// switch-host sockpair

					socketpair(AF_UNIX, SOCK_STREAM, 0, ihSockpair);
					socketpair(AF_UNIX, SOCK_STREAM, 0, shSockpair);

					hosts[hostIndex].interpreterFD = ihSockpair[1];
					hosts[hostIndex].interface = shSockpair[1];

					hostControlFDs[hostIndex] = ihSockpair[0];
					switches[switchIndex].interfaces[switchInterfaceIndex] = shSockpair[0];
				}

				switchInterfaceIndex++;
			}

			int srSockpair[2];				// switch-router sockpair

			socketpair(AF_UNIX, SOCK_STREAM, 0, srSockpair);

			switchInterfaceIndex

			switches[switchIndex].;

			routerInterfaceIndex++;
		}
	}

	return 0;
}
/* forking a host
HostInfo *tempHostInfo = malloc(sizeof(HostInfo));

				memcpy(tempHostInfo, &hosts[hostIndex], sizeof(HostInfo));

				int forkRC = fork();

				if(forkRC == 0)
				{
					close(ihSockpair[0]);
					close(shSockpair[0]);
					FreeOperationsMemory(operations, currMaxOperations);

					ActAsHost(tempHostInfo);

					close(ihSockpair[1]);
					close(shSockpair[1]);
					free(tempHostInfo);
					exit(0);
				}
				else
				{
					close(ihSockpair[1]);
					close(shSockpair[1]);
					free(tempHostInfo);
				}
*/
void FreeOperationsMemory(char ***operations, int currMaxOperations)
{
	for(int i = 0; i < OPERATIONS_START_ELEMENTS; i++)
	{
		for(int j = 0; j < MAX_LINE_TOKENS; j++)
		{
			free(operations[i][j]);
		}

		free(operations[i]);
	}

	free(operations);
}
