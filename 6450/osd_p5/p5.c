// TODO: what to do for
// SOCKPAIR SIDES
// 0 interpreter-switch 1
// 0 interpreter-host/router 1
// 0 switch-router/host 1
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <sys/wait.h>
#include <ctype.h>
#include <sys/time.h>

#define NAME_LENGTH 64
#define MAX_LINE_LENGTH 896
#define CONFIG_MAX_SIZE 42
#define OPERATIONS_START_ELEMENTS 20
#define MAX_LINE_TOKENS 14
#define MAX_TOKEN_LENGTH 64
#define EXTRA_OPERATIONS 10
#define MAX_ETHERNET_PACKET_SIZE 104
#define ETHERNET_PAYLOAD_SIZE 100
#define ARP_CACHE_SIZE 6
#define ROUTE_TABLE_SIZE 6
#define MAX_IP_PACKET_SIZE 100
#define IP_PAYLOAD_SIZE 93
#define MAX_TCP_PACKET_SIZE 93
#define TCP_PAYLOAD_SIZE 89
#define MAX_ACTIVE_PORTS 4
#define MAX_TCP_MESSAGES 8

#define FINISHED_FLAG 1
#define MACSEND_FLAG 2
#define ARPTEST_FLAG 3
#define ARPPRT_FLAG 4
#define ROUTE_FLAG 5
#define PING_FLAG 6
#define TR_FLAG 7
#define TCP_TEST_FLAG 8
#define TCP_CONN_FLAG 9
#define TCP_SEND_FLAG 10
#define TCP_DONE_FLAG 11

#define MACSEND_PROTOCOL 0
#define ARPREQ_PROTOCOL 1
#define ARPREPLY_PROTOCOL 2
#define PING_PROTOCOL 3
#define TR_PROTOCOL 4
#define TCP_TEST_PROTOCOL 5
#define TCP_CONN_PROTOCOL 6
#define TCP_SEND_PROTOCOL 7
#define TCP_DONE_PROTOCOL 8

#define DEF_ROUTE 255

// switch information
typedef struct {

char *name;
unsigned char netNumber;
unsigned char knownMACs[6];
int interfaces[6];
int interpreterFD;

} SwitchInfo;

// router information
typedef struct {

char *name;
unsigned char MACs[6];
unsigned char netIPs[6];
unsigned char hostIPs[6];
int interfaces[6];
int interpreterFD;
unsigned char arpCacheNet[6];
unsigned char arpCacheHost[6];
unsigned char arpCacheMAC[6];
int arpCacheIndex;
int arpCacheCount;
unsigned char routeTableDestNet[6];
unsigned char routeTableSourceMAC[6];
unsigned char routeTableGateNet[6];
unsigned char routeTableGateHost[6];
int routeTableCount;

} RouterInfo;

// host information
typedef struct {

char *name;
unsigned char MAC;
unsigned char netIP;
unsigned char hostIP;
int interface;
int interpreterFD;
unsigned char arpCacheNet[6];
unsigned char arpCacheHost[6];
unsigned char arpCacheMAC[6];
int arpCacheIndex;
int arpCacheCount;
unsigned char routeTableDestNet[6];
unsigned char routeTableSourceMAC[6];
unsigned char routeTableGateNet[6];
unsigned char routeTableGateHost[6];
int routeTableCount;

} HostInfo;

void FreeOperationsMemory(char ***operations, int currMaxOperations);
void ActAsSwitch(SwitchInfo *switchInfo);
void ActAsRouter(RouterInfo *routerInfo);
void ActAsHost(HostInfo *hostInfo);
unsigned char *CreateEthernetPacket(unsigned char destMAC, unsigned char srcMAC, unsigned char type, unsigned char length, char *payload);
int GetEthernetPacketDestMAC(unsigned char *packet);
int GetEthernetPacketSourceMAC(unsigned char *packet);
char *GetPayload(char *packet);
char *GetIPPayload(char *packet);
char *GetTCPPayload(char *packet);
unsigned char *CreateIPPacket(unsigned char length, unsigned char ttl, unsigned char protocol, unsigned char sourceNet, unsigned char sourceHost, unsigned char destNet, unsigned char destHost, unsigned char *payload);

int main(int argc, char *argv[])
{
	setbuf(stdout, NULL);

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

		if(!configDone && line[0] != 'v' && line[0] != '\0' && !isspace(line[0]))
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

			int isRoute = 0;

			if(p = strtok(line, " \t"))
			{
				strcpy(operations[lineCount][0], p);

				if(strcmp(operations[lineCount][0], "route") == 0)
					isRoute = 1;

				if(strcmp(operations[lineCount][0], "prt") == 0)
				{
					if(p = strtok(NULL, "\n"))
						strcpy(operations[lineCount][1], p);
					else
						operations[lineCount][tokenIndex][0] = '\n';
				}
				else
				{
					for(tokenIndex = 1; tokenIndex < MAX_LINE_TOKENS; tokenIndex++)
					{
						if(p = strtok(NULL, " \t"))
							strcpy(operations[lineCount][tokenIndex], p);
						else
							break;
					}

					if(isRoute && tokenIndex <= 4)
					{
						// put in a default gateway
						strcpy(operations[lineCount][tokenIndex], "255.255");
					}
				}

				isRoute = 0;
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
	int switchControlFDs[6];
	int routerControlFDs[36];
	int hostControlFDs[36];

	for(int i = 0; i < numConfig; i++)
	{
		switch(config[i][0][1])
		{
			case 's':
				// TODO: free
				switches[numSwitches].name = malloc(NAME_LENGTH * sizeof(char));
				strcpy(switches[numSwitches].name, config[i][1]);
				switches[numSwitches].netNumber = (unsigned char)atoi(config[i][2]);
				numSwitches++;
				break;
			case 'r':
				// TODO: free
				routers[numRouters].name = malloc(NAME_LENGTH * sizeof(char));
				strcpy(routers[numRouters].name, config[i][1]);
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
				// TODO: free
				hosts[numHosts].name = malloc(NAME_LENGTH * sizeof(char));
				strcpy(hosts[numHosts].name, config[i][1]);
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

	int fdsOpen = 3;	// 0, 1, 2 are stdin/out/err

	// add switch known macs and set up sockpairs
	for(int i = 0; i < numSwitches; i++)
	{
		int isSockpair[2];		// interpreter-switch socketpair

		socketpair(AF_UNIX, SOCK_STREAM, 0, isSockpair);
		fdsOpen += 2;

		switchControlFDs[i] = isSockpair[0];
		switches[i].interpreterFD = isSockpair[1];

		int knownMACs = 0;
		for(int j = 0; j < numRouters; j++)
		{
			for(int k = 0; routers[j].netIPs[k] != 0; k++)
			{
				if(routers[j].netIPs[k] == switches[i].netNumber)
				{
					switches[i].knownMACs[knownMACs] = routers[j].MACs[k];

					int srSockpair[2];			// switch-router socketpair

					socketpair(AF_UNIX, SOCK_STREAM, 0, srSockpair);
					fdsOpen += 2;

					switches[i].interfaces[knownMACs] = srSockpair[0];
					routers[j].interfaces[k] = srSockpair[1];

					if(routers[j].interpreterFD == 0)
					{
						int irSockpair[2];			// interpreter-router socketpair

						socketpair(AF_UNIX, SOCK_STREAM, 0, irSockpair);
						fdsOpen += 2;

						routerControlFDs[j] = irSockpair[0];
						routers[j].interpreterFD = irSockpair[1];
					}

					knownMACs++;
				}
			}
		}

		for(int j = 0; j < numHosts; j++)
		{
			if(hosts[j].netIP == switches[i].netNumber)
			{
				switches[i].knownMACs[knownMACs] = hosts[j].MAC;

				int shSockpair[2];			// switch-host socketpair
				int ihSockpair[2];			// interpreter-host socketpair

				socketpair(AF_UNIX, SOCK_STREAM, 0, shSockpair);
				socketpair(AF_UNIX, SOCK_STREAM, 0, ihSockpair);
				fdsOpen += 4;

				switches[i].interfaces[knownMACs] = shSockpair[0];
				hosts[j].interface = shSockpair[1];

				hostControlFDs[j] = ihSockpair[0];
				hosts[j].interpreterFD = ihSockpair[1];

				knownMACs++;
			}
		}
	}

	for(int i = 0; i < numSwitches; i++)
	{
		SwitchInfo *tempSwitchInfo = malloc(sizeof(SwitchInfo));
		memcpy(tempSwitchInfo, &switches[i], sizeof(SwitchInfo));

		int forkRC = fork();

		setbuf(stdout, NULL);

		if(forkRC == 0)
		{
			for(int i = 3; i < fdsOpen; i++)
			{
				int isMine = 0;
				int interfaceIndex = 0;

				while(interfaceIndex < 6 && tempSwitchInfo->interfaces[interfaceIndex] != 0)
				{
					if(tempSwitchInfo->interfaces[interfaceIndex] == i)
					{
						isMine = 1;
						break;
					}

					interfaceIndex++;
				}

				if(tempSwitchInfo->interpreterFD == i)
					isMine = 1;

				if(!isMine)
					close(i);
			}
			
			FreeOperationsMemory(operations, currMaxOperations);
			// Can I free more stuff

			ActAsSwitch(tempSwitchInfo);

			int interfaceIndex = 0;

			while(interfaceIndex < 6 && tempSwitchInfo->interfaces[interfaceIndex] != 0)
			{
				close(tempSwitchInfo->interfaces[interfaceIndex]);
				interfaceIndex++;
			}

			free(tempSwitchInfo);
			
			exit(0);
		}
		else
		{
			int interfaceIndex = 0;

			while(interfaceIndex < 6 && switches[i].interfaces[interfaceIndex] != 0)
			{
				close(switches[i].interfaces[interfaceIndex]);
				interfaceIndex++;
			}

			close(switches[i].interpreterFD);
			
			free(tempSwitchInfo);
		}
	}

	for(int i = 0; i < numRouters; i++)
	{
		RouterInfo *tempRouterInfo = malloc(sizeof(RouterInfo));
		memcpy(tempRouterInfo, &routers[i], sizeof(RouterInfo));

		int forkRC = fork();

		setbuf(stdout, NULL);

		if(forkRC == 0)
		{
			for(int i = 3; i < fdsOpen; i++)
			{
				int isMine = 0;
				int interfaceIndex = 0;

				while(interfaceIndex < 6 && tempRouterInfo->interfaces[interfaceIndex] != 0)
				{
					if(tempRouterInfo->interfaces[interfaceIndex] == i || tempRouterInfo->interpreterFD == i)
					{
						isMine = 1;
						break;
					}

					interfaceIndex++;
				}

				if(!isMine)
					close(i);
			}

			FreeOperationsMemory(operations, currMaxOperations);
			// TODO: can I free more stuff...?

			ActAsRouter(tempRouterInfo);

			int interfaceIndex = 0;

			while(interfaceIndex < 6 && tempRouterInfo->interfaces[interfaceIndex] != 0)
			{
				close(tempRouterInfo->interfaces[interfaceIndex]);
				interfaceIndex++;
			}

			close(tempRouterInfo->interpreterFD);

			free(tempRouterInfo);

			exit(0);
		}
		else
		{
			int interfaceIndex = 0;

			while(interfaceIndex < 6 && routers[i].interfaces[interfaceIndex] != 0)
			{
				close(routers[i].interfaces[interfaceIndex]);
				interfaceIndex++;
			}

			close(routers[i].interpreterFD);

			free(tempRouterInfo);
		}
	}

	for(int i = 0; i < numHosts; i++)
	{
		HostInfo *tempHostInfo = malloc(sizeof(HostInfo));
		memcpy(tempHostInfo, &hosts[i], sizeof(HostInfo));

		int forkRC = fork();

		setbuf(stdout, NULL);

		if(forkRC == 0)
		{
			for(int i = 3; i < fdsOpen; i++)
			{
				if(i != tempHostInfo->interface && i != tempHostInfo->interpreterFD)
					close(i);
			}

			FreeOperationsMemory(operations, currMaxOperations);
			// TODO: can I free more stuff...?

			ActAsHost(tempHostInfo);

			close(tempHostInfo->interface);
			close(tempHostInfo->interpreterFD);

			free(tempHostInfo);

			exit(0);
		}
		else
		{
			close(hosts[i].interface);
			close(hosts[i].interpreterFD);

			free(tempHostInfo);
		}
	}

	// do the operations...
	int operationsIndex = 0;

	while(operationsIndex < currMaxOperations && operations[operationsIndex][0][0] != 0)
	{
		if(strcmp(operations[operationsIndex][0], "prt") == 0)
		{
			printf("%s\n", operations[operationsIndex][1]);
		}
		else if(strcmp(operations[operationsIndex][0], "pause") == 0)
		{
			if(strcmp(operations[operationsIndex][1], "tty") == 0)
			{
				printf("press return to continue\n");
			 	getchar();
			}
			else
			{
				int timeToWait = atoi(operations[operationsIndex][1]);
				sleep(timeToWait);
			}
		}
		else if(strcmp(operations[operationsIndex][0], "macsend") == 0)
		{
			unsigned char senderMAC = (unsigned char)atoi(operations[operationsIndex][2]);
			int sent = 0;
			int index = 0;

			while(index < numRouters)
			{
				int macIndex = 0;

				while(macIndex < 6 && routers[index].MACs[macIndex])
				{
					if(routers[index].MACs[macIndex] == senderMAC)
					{
						int routerInterface = routerControlFDs[index];
						unsigned char charBuffer[1];
						unsigned char buffer[100] = { 0 };

						// 2 means macsend
						charBuffer[0] = MACSEND_FLAG;
						write(routerInterface, (void *)&charBuffer, 1);

						// receiver
						charBuffer[0] = (unsigned char)atoi(operations[operationsIndex][3]);
						write(routerInterface, (void *)&charBuffer, 1);

						// sender
						charBuffer[0] = (unsigned char)atoi(operations[operationsIndex][2]);
						write(routerInterface, (void *)&charBuffer, 1);

						// message
						int messageLength = strlen(operations[operationsIndex][1]);
						strncpy(buffer, operations[operationsIndex][1], messageLength);

						int bytesWritten = write(routerInterface, (void *)&buffer, 100);

						while(bytesWritten < 100)
							bytesWritten += write(routerInterface, (void *)&buffer + bytesWritten, 100 - bytesWritten);

						sent = 1;
						break;
					}

					macIndex++;
				}

				if(sent)
					break;

				index++;
			}

			if(!sent)
			{
				index = 0;

				while(index < numHosts)
				{
					if(hosts[index].MAC == senderMAC)
					{
						int hostInterface = hostControlFDs[index];
						unsigned char charBuffer[1];
						unsigned char buffer[100];

						// 2 means macsend
						charBuffer[0] = MACSEND_FLAG;
						write(hostInterface, (void *)&charBuffer, 1);

						// receiver
						charBuffer[0] = (unsigned char)atoi(operations[operationsIndex][3]);
						write(hostInterface, (void *)&charBuffer, 1);

						// sender
						charBuffer[0] = (unsigned char)atoi(operations[operationsIndex][2]);
						write(hostInterface, (void *)&charBuffer, 1);

						// message
						int messageLength = strlen(operations[operationsIndex][1]);
						strncpy(buffer, operations[operationsIndex][1], messageLength);

						int bytesWritten = write(hostInterface, (void *)&buffer, 100);

						while(bytesWritten < 100)
							bytesWritten += write(hostInterface, (void *)&buffer + bytesWritten, 100 - bytesWritten);

						sent = 1;
						break;
					}
					
					index++;
				}
			}
		}
		else if(strcmp(operations[operationsIndex][0], "arptest") == 0)
		{
			// get the target host or router's name
			char senderName[NAME_LENGTH];
			strcpy(&senderName[0], operations[operationsIndex][1]);

			// find which host or router it is
			int deviceInterface = 0;

			for(int deviceIndex = 0; deviceIndex < numRouters; deviceIndex++)
			{
				if(strcmp(routers[deviceIndex].name, senderName) == 0)
				{
					deviceInterface = routerControlFDs[deviceIndex];
					break;
				}
			}

			if(!deviceInterface)
			{
				for(int deviceIndex = 0; deviceIndex < numHosts; deviceIndex++)
				{
					if(strcmp(hosts[deviceIndex].name, senderName) == 0)
					{
						deviceInterface = hostControlFDs[deviceIndex];
						break;
					}
				}
			}

			// send the sender the required information
			unsigned char charBuffer[1];

			// 3 means arptest
			charBuffer[0] = ARPTEST_FLAG;
			write(deviceInterface, (void *)&charBuffer, 1);

			// network part of IP
			char *p;
			if(p = strtok(operations[operationsIndex][2], "."))
			{
				charBuffer[0] = (unsigned char)atoi(p);
				write(deviceInterface, (void *)&charBuffer, 1);

				if(p = strtok(NULL, " \0"))
				{
					charBuffer[0] = (unsigned char)atoi(p);
					write(deviceInterface, (void *)&charBuffer, 1);
				}
			}
		}
		else if(strcmp(operations[operationsIndex][0], "arpprt") == 0)
		{
			// get the target host or router's name
			char targetName[NAME_LENGTH];
			strcpy(&targetName[0], operations[operationsIndex][1]);

			// find which host or router it is
			int deviceInterface = 0;

			for(int deviceIndex = 0; deviceIndex < numRouters; deviceIndex++)
			{
				if(strcmp(routers[deviceIndex].name, targetName) == 0)
				{
					deviceInterface = routerControlFDs[deviceIndex];
					break;
				}
			}

			if(!deviceInterface)
			{
				for(int deviceIndex = 0; deviceIndex < numHosts; deviceIndex++)
				{
					if(strcmp(hosts[deviceIndex].name, targetName) == 0)
					{
						deviceInterface = hostControlFDs[deviceIndex];
						break;
					}
				}
			}

			// tell the target to print its cache
			unsigned char charBuffer[1];

			// 4 means arpprt
			charBuffer[0] = ARPPRT_FLAG;
			write(deviceInterface, (void *)&charBuffer, 1);
		}
		else if(strcmp(operations[operationsIndex][0], "route") == 0)
		{
			// get the target host or router's name
			char targetName[NAME_LENGTH];
			strcpy(&targetName[0], operations[operationsIndex][1]);

			// find which host or router it is
			int deviceInterface = 0;

			for(int deviceIndex = 0; deviceIndex < numRouters; deviceIndex++)
			{
				if(strcmp(routers[deviceIndex].name, targetName) == 0)
				{
					deviceInterface = routerControlFDs[deviceIndex];
					break;
				}
			}

			if(!deviceInterface)
			{
				for(int deviceIndex = 0; deviceIndex < numHosts; deviceIndex++)
				{
					if(strcmp(hosts[deviceIndex].name, targetName) == 0)
					{
						deviceInterface = hostControlFDs[deviceIndex];
						break;
					}
				}
			}

			// tell the target we're sending routing info
			unsigned char charBuffer[1];
			charBuffer[0] = ROUTE_FLAG;
			write(deviceInterface, (void *)&charBuffer, 1);

			// tell the target the destination network
			if(strcmp(operations[operationsIndex][2], "def") == 0)
			{
				charBuffer[0] = DEF_ROUTE;
			}
			else
			{
				charBuffer[0] = (unsigned char)atoi(operations[operationsIndex][2]);
			}
			write(deviceInterface, (void *)&charBuffer, 1);

			// tell the target the source MAC
			charBuffer[0] = (unsigned char)atoi(operations[operationsIndex][3]);
			write(deviceInterface, (void *)&charBuffer, 1);
			
			// IP
			char *p;
			if(p = strtok(operations[operationsIndex][4], "."))
			{
				// network part of IP
				charBuffer[0] = (unsigned char)atoi(p);
				write(deviceInterface, (void *)&charBuffer, 1);

				if(p = strtok(NULL, " \0"))
				{
					// host part of IP
					charBuffer[0] = (unsigned char)atoi(p);
					write(deviceInterface, (void *)&charBuffer, 1);
				}
			}
		}
		else if(strcmp(operations[operationsIndex][0], "iptest") == 0)
		{
			// get the target host or router's name
			char targetName[NAME_LENGTH];
			strcpy(&targetName[0], operations[operationsIndex][1]);

			// find which host or router it is
			int deviceInterface = 0;

			for(int deviceIndex = 0; deviceIndex < numRouters; deviceIndex++)
			{
				if(strcmp(routers[deviceIndex].name, targetName) == 0)
				{
					deviceInterface = routerControlFDs[deviceIndex];
					break;
				}
			}

			if(!deviceInterface)
			{
				for(int deviceIndex = 0; deviceIndex < numHosts; deviceIndex++)
				{
					if(strcmp(hosts[deviceIndex].name, targetName) == 0)
					{
						deviceInterface = hostControlFDs[deviceIndex];
						break;
					}
				}
			}

			// tell the target to do an iptest
			unsigned char charBuffer[1];
			charBuffer[0] = PING_FLAG;
			write(deviceInterface, (void *)&charBuffer, 1);

			// tell the target who to look for (either a host starting with h or an IP)
			if(operations[operationsIndex][2][0] == 'h')
			{
				int toHostIndex;

				for(toHostIndex = 0; toHostIndex < numHosts; toHostIndex++)
				{
					if(strcmp(hosts[toHostIndex].name, operations[operationsIndex][2]) == 0)
						break;
				}

				charBuffer[0] = hosts[toHostIndex].netIP;
				write(deviceInterface, (void *)&charBuffer, 1);

				charBuffer[0] = hosts[toHostIndex].hostIP;
				write(deviceInterface, (void *)&charBuffer, 1);
			}
			else
			{
				char *p;
				if(p = strtok(operations[operationsIndex][2], "."))
				{
					// network part of IP
					charBuffer[0] = (unsigned char)atoi(p);
					write(deviceInterface, (void *)&charBuffer, 1);

					if(p = strtok(NULL, " \0"))
					{
						// host part of IP
						charBuffer[0] = (unsigned char)atoi(p);
						write(deviceInterface, (void *)&charBuffer, 1);
					}
				}
			}
		}
		else if(strcmp(operations[operationsIndex][0], "trtest") == 0)
		{
			// get the target host or router's name
			char targetName[NAME_LENGTH];
			strcpy(&targetName[0], operations[operationsIndex][1]);

			// find which host or router it is
			int deviceInterface = 0;

			for(int deviceIndex = 0; deviceIndex < numRouters; deviceIndex++)
			{
				if(strcmp(routers[deviceIndex].name, targetName) == 0)
				{
					deviceInterface = routerControlFDs[deviceIndex];
					break;
				}
			}

			if(!deviceInterface)
			{
				for(int deviceIndex = 0; deviceIndex < numHosts; deviceIndex++)
				{
					if(strcmp(hosts[deviceIndex].name, targetName) == 0)
					{
						deviceInterface = hostControlFDs[deviceIndex];
						break;
					}
				}
			}

			// tell the target to do a trtest
			unsigned char charBuffer[1];
			charBuffer[0] = TR_FLAG;
			write(deviceInterface, (void *)&charBuffer, 1);

			// tell the target who to look for (either a host starting with h or an IP)
			if(operations[operationsIndex][2][0] == 'h')
			{
				int toHostIndex;

				for(toHostIndex = 0; toHostIndex < numHosts; toHostIndex++)
				{
					if(strcmp(hosts[toHostIndex].name, operations[operationsIndex][2]) == 0)
						break;
				}

				charBuffer[0] = hosts[toHostIndex].netIP;
				write(deviceInterface, (void *)&charBuffer, 1);

				charBuffer[0] = hosts[toHostIndex].hostIP;
				write(deviceInterface, (void *)&charBuffer, 1);
			}
			else
			{
				char *p;
				if(p = strtok(operations[operationsIndex][2], "."))
				{
					// network part of IP
					charBuffer[0] = (unsigned char)atoi(p);
					write(deviceInterface, (void *)&charBuffer, 1);

					if(p = strtok(NULL, " \0"))
					{
						// host part of IP
						charBuffer[0] = (unsigned char)atoi(p);
						write(deviceInterface, (void *)&charBuffer, 1);
					}
				}
			}
		}
		else if(strcmp(operations[operationsIndex][0], "tcptest") == 0)
		{
			// get the target host or router's name
			char targetName[NAME_LENGTH];
			strcpy(&targetName[0], operations[operationsIndex][1]);

			// find which host or router it is
			int deviceInterface = 0;

			for(int deviceIndex = 0; deviceIndex < numRouters; deviceIndex++)
			{
				if(strcmp(routers[deviceIndex].name, targetName) == 0)
				{
					deviceInterface = routerControlFDs[deviceIndex];
					break;
				}
			}

			if(!deviceInterface)
			{
				for(int deviceIndex = 0; deviceIndex < numHosts; deviceIndex++)
				{
					if(strcmp(hosts[deviceIndex].name, targetName) == 0)
					{
						deviceInterface = hostControlFDs[deviceIndex];
						break;
					}
				}
			}

			// tell the target to do a tcptest
			unsigned char charBuffer[1];
			charBuffer[0] = TCP_TEST_FLAG;
			write(deviceInterface, (void *)&charBuffer, 1);

			// wait for it to say it's ready
			read(deviceInterface, (void *)&charBuffer, 1);

			// tell the target who to look for
			int toHostIndex;

			for(toHostIndex = 0; toHostIndex < numHosts; toHostIndex++)
			{
				if(strcmp(hosts[toHostIndex].name, operations[operationsIndex][2]) == 0)
					break;
			}

			charBuffer[0] = hosts[toHostIndex].netIP;
			write(deviceInterface, (void *)&charBuffer, 1);

			charBuffer[0] = hosts[toHostIndex].hostIP;
			write(deviceInterface, (void *)&charBuffer, 1);

			// tell the target what port use
			int port = atoi(operations[operationsIndex][3]);
			
			write(deviceInterface, (void *)&port, sizeof(int));

			// tell the target the payload
			unsigned char payload[100];

			memcpy((void *)&payload, (void *)operations[operationsIndex][4], strlen(operations[operationsIndex][4]));

			write(deviceInterface, (void *)&payload, strlen(operations[operationsIndex][4]));
		}
		else if(strcmp(operations[operationsIndex][0], "ftpd_conn") == 0)
		{
			// get the target host or router's name
			char targetName[NAME_LENGTH];
			strcpy(&targetName[0], operations[operationsIndex][1]);

			// find which host or router it is
			int deviceInterface = 0;

			for(int deviceIndex = 0; deviceIndex < numRouters; deviceIndex++)
			{
				if(strcmp(routers[deviceIndex].name, targetName) == 0)
				{
					deviceInterface = routerControlFDs[deviceIndex];
					break;
				}
			}

			if(!deviceInterface)
			{
				for(int deviceIndex = 0; deviceIndex < numHosts; deviceIndex++)
				{
					if(strcmp(hosts[deviceIndex].name, targetName) == 0)
					{
						deviceInterface = hostControlFDs[deviceIndex];
						break;
					}
				}
			}

			// tell the target to do a ftpd_conn
			unsigned char charBuffer[1];
			charBuffer[0] = TCP_CONN_FLAG;
			write(deviceInterface, (void *)&charBuffer, 1);

			// wait for it to say it's ready
			read(deviceInterface, (void *)&charBuffer, 1);

			// tell the target who to look for
			int toHostIndex;

			for(toHostIndex = 0; toHostIndex < numHosts; toHostIndex++)
			{
				if(strcmp(hosts[toHostIndex].name, operations[operationsIndex][2]) == 0)
					break;
			}

			charBuffer[0] = hosts[toHostIndex].netIP;
			write(deviceInterface, (void *)&charBuffer, 1);

			charBuffer[0] = hosts[toHostIndex].hostIP;
			write(deviceInterface, (void *)&charBuffer, 1);

			// tell the target what port use
			int port = atoi(operations[operationsIndex][3]);
			
			write(deviceInterface, (void *)&port, sizeof(int));
		}
		else if(strcmp(operations[operationsIndex][0], "ftp_send") == 0)
		{
			// get the target host or router's name
			char targetName[NAME_LENGTH];
			strcpy(&targetName[0], operations[operationsIndex][1]);

			// find which host or router it is
			int deviceInterface = 0;

			for(int deviceIndex = 0; deviceIndex < numRouters; deviceIndex++)
			{
				if(strcmp(routers[deviceIndex].name, targetName) == 0)
				{
					deviceInterface = routerControlFDs[deviceIndex];
					break;
				}
			}

			if(!deviceInterface)
			{
				for(int deviceIndex = 0; deviceIndex < numHosts; deviceIndex++)
				{
					if(strcmp(hosts[deviceIndex].name, targetName) == 0)
					{
						deviceInterface = hostControlFDs[deviceIndex];
						break;
					}
				}
			}

			// tell the target to do a ftp_send
			unsigned char charBuffer[1];
			charBuffer[0] = TCP_SEND_FLAG;
			write(deviceInterface, (void *)&charBuffer, 1);

			// wait for it to say it's ready
			read(deviceInterface, (void *)&charBuffer, 1);

			// tell the target who to look for
			int toHostIndex;

			for(toHostIndex = 0; toHostIndex < numHosts; toHostIndex++)
			{
				if(strcmp(hosts[toHostIndex].name, operations[operationsIndex][2]) == 0)
					break;
			}

			charBuffer[0] = hosts[toHostIndex].netIP;
			write(deviceInterface, (void *)&charBuffer, 1);

			charBuffer[0] = hosts[toHostIndex].hostIP;
			write(deviceInterface, (void *)&charBuffer, 1);

			// tell the target what port use
			int port = atoi(operations[operationsIndex][3]);
			
			write(deviceInterface, (void *)&port, sizeof(int));

			// tell the target the payload
			unsigned char payload[100];

			memcpy((void *)&payload, (void *)operations[operationsIndex][4], strlen(operations[operationsIndex][4]));

			write(deviceInterface, (void *)&payload, strlen(operations[operationsIndex][4]));
		}
		else if(strcmp(operations[operationsIndex][0], "ftp_done") == 0)
		{
			// get the target host or router's name
			char targetName[NAME_LENGTH];
			strcpy(&targetName[0], operations[operationsIndex][1]);

			// find which host or router it is
			int deviceInterface = 0;

			for(int deviceIndex = 0; deviceIndex < numRouters; deviceIndex++)
			{
				if(strcmp(routers[deviceIndex].name, targetName) == 0)
				{
					deviceInterface = routerControlFDs[deviceIndex];
					break;
				}
			}

			if(!deviceInterface)
			{
				for(int deviceIndex = 0; deviceIndex < numHosts; deviceIndex++)
				{
					if(strcmp(hosts[deviceIndex].name, targetName) == 0)
					{
						deviceInterface = hostControlFDs[deviceIndex];
						break;
					}
				}
			}

			// tell the target to do a ftp_done
			unsigned char charBuffer[1];
			charBuffer[0] = TCP_DONE_FLAG;
			write(deviceInterface, (void *)&charBuffer, 1);

			// wait for it to say it's ready
			read(deviceInterface, (void *)&charBuffer, 1);

			// tell the target who to look for
			int toHostIndex;

			for(toHostIndex = 0; toHostIndex < numHosts; toHostIndex++)
			{
				if(strcmp(hosts[toHostIndex].name, operations[operationsIndex][2]) == 0)
					break;
			}

			charBuffer[0] = hosts[toHostIndex].netIP;
			write(deviceInterface, (void *)&charBuffer, 1);

			charBuffer[0] = hosts[toHostIndex].hostIP;
			write(deviceInterface, (void *)&charBuffer, 1);

			// tell the target what port use
			int port = atoi(operations[operationsIndex][3]);
			
			write(deviceInterface, (void *)&port, sizeof(int));
		}

		operationsIndex++;
	}

	#if DEBUG
	int debug = 1;
	while(debug)
		;
	#endif

	char messageFlagBuffer[1];
	messageFlagBuffer[0] = 1;

	for(int i = 0; i < numSwitches; i++)
	{
		write(switchControlFDs[i], (void *)&messageFlagBuffer, 1);
	}

	for(int i = 0; i < numRouters; i++)
	{
		write(routerControlFDs[i], (void *)&messageFlagBuffer, 1);
	}

	for(int i = 0; i < numHosts; i++)
	{
		write(hostControlFDs[i], (void *)&messageFlagBuffer, 1);
	}

	int waitStatus;

	while(waitpid(-1, &waitStatus, 0))
		if(errno == ECHILD)
			break;

	return 0;
}
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

void ActAsSwitch(SwitchInfo *switchInfo)
{
	#if DEBUG
	int debug = 1;
	while(debug)
		;
	#endif

	int done = 0;
	int doneWithInterface[6] = { 0 };
	int rc;
	int bytesRead;
	int setSize = 0;
	fd_set readFDs;
	unsigned char buffer[MAX_ETHERNET_PACKET_SIZE];

	while(!done)
	{
		FD_ZERO(&readFDs);
		
		int interfaceIndex = 0;

		while(interfaceIndex < 6 && switchInfo->interfaces[interfaceIndex])
		{
			if(!doneWithInterface[interfaceIndex])
			{
				FD_SET(switchInfo->interfaces[interfaceIndex], &readFDs);

				if(setSize < switchInfo->interfaces[interfaceIndex])
					setSize = switchInfo->interfaces[interfaceIndex];
			}

			interfaceIndex++;
		}

		FD_SET(switchInfo->interpreterFD, &readFDs);

		if(setSize < switchInfo->interpreterFD)
			setSize = switchInfo->interpreterFD;

		setSize++;

		rc = select(setSize, &readFDs, NULL, NULL, NULL);

		if(rc == -1 && errno == EINTR)
			continue;

		interfaceIndex = 0;
		int fdWasSet = 0;

		while(interfaceIndex < 6 && switchInfo->interfaces[interfaceIndex])
		{
			if(FD_ISSET(switchInfo->interfaces[interfaceIndex], &readFDs))
			{
				fdWasSet = 1;
				bytesRead = read(switchInfo->interfaces[interfaceIndex], (void *)&buffer, MAX_ETHERNET_PACKET_SIZE);

				if(!bytesRead)
				{
					doneWithInterface[interfaceIndex] = 1;

					break;
				}

				while(bytesRead < MAX_ETHERNET_PACKET_SIZE)
					bytesRead += read(switchInfo->interfaces[interfaceIndex], (void *)&buffer + bytesRead, MAX_ETHERNET_PACKET_SIZE - bytesRead);

				int destInterfaceIndex = 0;

				if(GetEthernetPacketDestMAC(buffer) == 255)
				{
					while(destInterfaceIndex < 6 && switchInfo->interfaces[destInterfaceIndex])
					{
						int bytesWritten = 0;
						
						bytesWritten = write(switchInfo->interfaces[destInterfaceIndex], (void *)&buffer, MAX_ETHERNET_PACKET_SIZE);

						while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
							bytesWritten += write(switchInfo->interfaces[destInterfaceIndex], (void *)&buffer + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

						destInterfaceIndex++;
					}

					break;
				}

				int foundDest = 0;

				while(destInterfaceIndex < 6 && switchInfo->interfaces[destInterfaceIndex])
				{
					if(switchInfo->knownMACs[destInterfaceIndex] == GetEthernetPacketDestMAC(buffer))
					{
						foundDest = 1;

						int bytesWritten = 0;

						bytesWritten = write(switchInfo->interfaces[destInterfaceIndex], (void *)&buffer, MAX_ETHERNET_PACKET_SIZE);

						while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
							bytesWritten += write(switchInfo->interfaces[destInterfaceIndex], (void *)&buffer + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

						break;
					}

					destInterfaceIndex++;
				}

				if(!foundDest)
					printf("%s: discarded a packet for unknown macaddr %d\n", switchInfo->name, (int)GetEthernetPacketDestMAC(buffer));
			}

			interfaceIndex++;
		}

		if(!fdWasSet && FD_ISSET(switchInfo->interpreterFD, &readFDs))
		{
			// interpreter will only ever send a single byte to a switch... 1 means we're done
			unsigned char buffer[1];

			read(switchInfo->interpreterFD, (void *)&buffer, 1);

			if(buffer[0] = FINISHED_FLAG)
			{
				done = 1;
			}
		}
	}
}

void ActAsRouter(RouterInfo *routerInfo)
{
	#if DEBUG
	int debug = 1;
	while(debug)
	 	;
	#endif

	int done = 0;
	int doneWithInterface[6] = { 0 };
	int rc;
	int bytesRead;
	int setSize = 0;
	fd_set readFDs;
	unsigned char buffer[MAX_ETHERNET_PACKET_SIZE];

	int expectingARPReply = 0;
	struct timeval arpStartTime;
	struct timeval oneSec;
	oneSec.tv_usec = 1000;
	struct timeval timePassed;
	unsigned char arpWaitNet;
	unsigned char arpWaitHost;
	int arpWaitHavePacket;
	unsigned char arpWaitSourceMAC;
	unsigned char arpWaitProtocol;
	int arpWaitFinalHost;
	int arpWaitFinalNet;
	int arpWaitSendInterface;
	int arpShouldPrint;
	unsigned char *arpWaitIPPacket;

	while(!done)
	{
		FD_ZERO(&readFDs);

		int interfaceIndex = 0;
		
		while(interfaceIndex < 6 && routerInfo->interfaces[interfaceIndex])
		{
			if(!doneWithInterface[interfaceIndex])
			{
				FD_SET(routerInfo->interfaces[interfaceIndex], &readFDs);

				if(setSize < routerInfo->interfaces[interfaceIndex])
					setSize = routerInfo->interfaces[interfaceIndex];
			}

			interfaceIndex++;
		}

		FD_SET(routerInfo->interpreterFD, &readFDs);

		if(setSize < routerInfo->interpreterFD)
			setSize = routerInfo->interpreterFD;

		setSize++;

		if(!expectingARPReply)
		{
			rc = select(setSize, &readFDs, NULL, NULL, NULL);
		}
		else
		{
			gettimeofday(&timePassed, NULL);
			timePassed.tv_sec -= arpStartTime.tv_sec;
			timePassed.tv_usec -= arpStartTime.tv_usec;

			if(timePassed.tv_sec >= 1 || timePassed.tv_usec > 1000)
			{
				// printf("%s: arpreq to %d.%d timed out\n", routerInfo->name, arpWaitNet, arpWaitHost);
				expectingARPReply = 0;
				arpWaitHavePacket = 0;
				free(arpWaitIPPacket);
				rc = select(setSize, &readFDs, NULL, NULL, NULL);
			}
			else
			{
				struct timeval tv;
				tv.tv_usec = oneSec.tv_usec - timePassed.tv_usec;

				rc = select(setSize, &readFDs, NULL, NULL, &tv);
			}
		}

		if(rc == -1 && errno == EINTR)
			continue;

		interfaceIndex = 0;
		int fdWasSet = 0;

		while(interfaceIndex < 6 && routerInfo->interfaces[interfaceIndex])
		{
			if(FD_ISSET(routerInfo->interfaces[interfaceIndex], &readFDs))
			{
				fdWasSet = 1;

				bytesRead = read(routerInfo->interfaces[interfaceIndex], (void *)&buffer, MAX_ETHERNET_PACKET_SIZE);

				if(!bytesRead)
				{
					doneWithInterface[interfaceIndex] = 1;
					break;
				}

				while(bytesRead < MAX_ETHERNET_PACKET_SIZE)
					bytesRead += read(routerInfo->interfaces[interfaceIndex], (void *)&buffer + bytesRead, MAX_ETHERNET_PACKET_SIZE - bytesRead);

				char *payload = GetPayload((char *)buffer);
				if(buffer[2] == MACSEND_PROTOCOL)
					printf("%s: macsend from %d on %d: %s\n", routerInfo->name, GetEthernetPacketSourceMAC(buffer), (int)routerInfo->MACs[interfaceIndex], payload);
				else if(buffer[2] == ARPREQ_PROTOCOL)
				{
					// check if they're asking for me
					if(payload[0] == routerInfo->netIPs[interfaceIndex] && payload[1] == routerInfo->hostIPs[interfaceIndex])
					{
						// printf("%s: arpreq from %d on %d: %d.%d\n", routerInfo->name, GetEthernetPacketSourceMAC(buffer), (int)routerInfo->MACs[interfaceIndex], payload[0], payload[1]);

						unsigned char message[100] = { 0 };
						message[0] = payload[0];
						message[1] = payload[1];
						message[2] = routerInfo->MACs[interfaceIndex];

						char *packetToSend = CreateEthernetPacket(GetEthernetPacketSourceMAC(buffer),
																  routerInfo->MACs[interfaceIndex],
																  (unsigned char)2,
																  4 + ETHERNET_PAYLOAD_SIZE,
																  message);

						int bytesWritten = write(routerInfo->interfaces[interfaceIndex], (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

						while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
							bytesWritten += write(routerInfo->interfaces[interfaceIndex], (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);
							
						// printf("%s: arpreply to %d on %d: %d.%d %d\n", routerInfo->name, GetEthernetPacketSourceMAC(buffer), routerInfo->MACs[interfaceIndex], message[0], message[1], message[2]);
					}
				}
				else if(buffer[2] == ARPREPLY_PROTOCOL)
				{
					// printf("%s: arpreply from %d on %d: %d.%d %d\n", routerInfo->name, GetEthernetPacketSourceMAC(buffer), routerInfo->MACs[interfaceIndex], payload[0], payload[1], payload[2]);

					// stick the correct info in cache
					int index = routerInfo->arpCacheIndex;
					routerInfo->arpCacheNet[index] = payload[0];
					routerInfo->arpCacheHost[index] = payload[1];
					routerInfo->arpCacheMAC[index] = payload[2];

					routerInfo->arpCacheIndex++;
					
					if(routerInfo->arpCacheCount != ARP_CACHE_SIZE)
						routerInfo->arpCacheCount++;

					if(routerInfo->arpCacheIndex == ARP_CACHE_SIZE)
						routerInfo->arpCacheIndex = 0;

					if(arpWaitHavePacket)
					{
						char *packetToSend = CreateEthernetPacket(routerInfo->arpCacheMAC[index],
																  arpWaitSourceMAC,
																  arpWaitProtocol,
																  4 + ETHERNET_PAYLOAD_SIZE,
																  arpWaitIPPacket);

						int bytesWritten = write(arpWaitSendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

						while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
							bytesWritten += write(arpWaitSendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

						if(arpShouldPrint)
						{
							if(arpWaitProtocol == PING_PROTOCOL)
								printf("%s: sent ping to %d.%d\n", routerInfo->name, arpWaitFinalNet, arpWaitFinalHost);
							else if(arpWaitProtocol == TR_PROTOCOL)
								printf("%s: sent traceroute to %d.%d\n", routerInfo->name, arpWaitFinalNet, arpWaitFinalHost);
						}

						free(arpWaitIPPacket);
						free(packetToSend);
					}
					
					expectingARPReply = 0;
					arpWaitHavePacket = 0;
				}
				else if(buffer[2] == PING_PROTOCOL)
				{
					int index = 0;
					int forMe = 0;

					while(index < 6 && routerInfo->interfaces[index])
					{
						if(payload[5] == routerInfo->netIPs[index] && payload[6] == routerInfo->hostIPs[index])
						{
							forMe = 1;
							break;
						}

						index++;
					}

					if(forMe)
					{
						printf("%s: received ping from %d.%d\n", routerInfo->name, payload[3], payload[4]);
					}
					else
					{
						unsigned char targetNet[1];
						unsigned char targetHost[1];

						targetNet[0] = payload[5];
						targetHost[0] = payload[6];
						int targetRouteIndex = -1;

						for(int i = 0; i < routerInfo->routeTableCount; i++)
						{
							if(routerInfo->routeTableDestNet[i] == targetNet[0] || routerInfo->routeTableDestNet[i] == DEF_ROUTE)
							{
								targetRouteIndex = i;
								break;
							}
						}

						if(targetRouteIndex == -1)
						{
							printf("%s: **** no route to IP: %d.%d\n", routerInfo->name, targetNet[0], targetHost[0]);
						}
						else
						{
							// find the right mac
							unsigned char sourceMAC = routerInfo->routeTableSourceMAC[targetRouteIndex];
							int interfaceIndex = 0;
							int sendInterface;

							while(interfaceIndex < 6 && routerInfo->interfaces[interfaceIndex])
							{
								if(routerInfo->MACs[interfaceIndex] == sourceMAC)
								{
									sendInterface = routerInfo->interfaces[interfaceIndex];
									break;
								}

								interfaceIndex++;
							}
			
							char *ipPayload = "";
							char *ipPacket = CreateIPPacket(7 + IP_PAYLOAD_SIZE,
															6,
															PING_PROTOCOL,
															payload[3],
															payload[4],
															targetNet[0],
															targetHost[0],
															ipPayload);

							unsigned char targetGateNet = routerInfo->routeTableGateNet[targetRouteIndex];
							unsigned char targetGateHost = routerInfo->routeTableGateHost[targetRouteIndex];

							if(targetGateNet == 255)
							{
								targetGateNet = targetNet[0];
								targetGateHost = targetHost[0];
							}

							int targetArpIndex = -1;

							for(int i = 0; i < routerInfo->arpCacheCount; i++)
							{
								if(routerInfo->arpCacheNet[i] == routerInfo->routeTableGateNet[targetRouteIndex] && routerInfo->arpCacheHost[i] == routerInfo->routeTableGateHost[targetRouteIndex])
								{
									targetArpIndex = i;
									break;
								}
							}

							if(targetArpIndex = -1)
							{
								// broadcast the request
								unsigned char message[ETHERNET_PAYLOAD_SIZE];
								message[0] = targetGateNet;
								message[1] = targetGateHost;

								char *packetToSend = CreateEthernetPacket(255,
																		  routerInfo->MACs[interfaceIndex],
																		  (unsigned char)1,
																		  4 + ETHERNET_PAYLOAD_SIZE,
																		  message);

								int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

								while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
									bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

								free(packetToSend);

								arpWaitNet = targetGateNet;
								arpWaitHost = targetGateHost;
								arpWaitHavePacket = 1;
								arpWaitIPPacket = ipPacket;
								arpWaitSourceMAC = sourceMAC;
								arpWaitProtocol = (unsigned char)PING_PROTOCOL;
								arpWaitFinalNet = targetNet[0];
								arpWaitFinalHost = targetHost[0];
								arpWaitSendInterface = sendInterface;
								arpShouldPrint = 0;
								expectingARPReply = 1;
								gettimeofday(&arpStartTime, NULL);
							}
							else
							{
								char *packetToSend = CreateEthernetPacket(routerInfo->arpCacheMAC[targetArpIndex],
																		  sourceMAC,
																		  (unsigned char)PING_PROTOCOL,
																		  4 + MAX_IP_PACKET_SIZE,
																		  ipPacket);

								int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

								while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
									bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

								free(ipPacket);
								free(packetToSend);
							}
						}
					}
				}
				else if(buffer[2] == TR_PROTOCOL)
				{
					int index = 0;
					int forMe = 0;

					while(index < 6 && routerInfo->interfaces[index])
					{
						if(payload[5] == routerInfo->netIPs[index] && payload[6] == routerInfo->hostIPs[index])
						{
							forMe = 1;
							break;
						}

						index++;
					}

					if(forMe)
					{
						printf("%s: received traceroute from %d.%d\nroute: %s\n", routerInfo->name, payload[3], payload[4], payload[7]);
					}
					else
					{
						unsigned char targetNet[1];
						unsigned char targetHost[1];

						int startOfNewNode = 7 + (3 * (6 - payload[1]));
						payload[startOfNewNode] = routerInfo->name[0];
						payload[startOfNewNode + 1] = routerInfo->name[1];
						payload[startOfNewNode + 2] = ' ';
						payload[1] = payload[1] - 1;

						targetNet[0] = payload[5];
						targetHost[0] = payload[6];
						int targetRouteIndex = -1;

						for(int i = 0; i < routerInfo->routeTableCount; i++)
						{
							if(routerInfo->routeTableDestNet[i] == targetNet[0] || routerInfo->routeTableDestNet[i] == DEF_ROUTE)
							{
								targetRouteIndex = i;
								break;
							}
						}

						if(targetRouteIndex == -1)
						{
							printf("%s: **** no route to IP: %d.%d\n", routerInfo->name, targetNet[0], targetHost[0]);
						}
						else
						{
							// find the right mac
							unsigned char sourceMAC = routerInfo->routeTableSourceMAC[targetRouteIndex];
							int interfaceIndex = 0;
							int sendInterface;

							while(interfaceIndex < 6 && routerInfo->interfaces[interfaceIndex])
							{
								if(routerInfo->MACs[interfaceIndex] == sourceMAC)
								{
									sendInterface = routerInfo->interfaces[interfaceIndex];
									break;
								}

								interfaceIndex++;
							}
			
							char *ipPayload = calloc(MAX_IP_PACKET_SIZE - 7, sizeof(char));
							strncpy(ipPayload, &payload[7], MAX_IP_PACKET_SIZE - 7);
							char *ipPacket = CreateIPPacket(7 + IP_PAYLOAD_SIZE,
															payload[1],
															TR_PROTOCOL,
															payload[3],
															payload[4],
															targetNet[0],
															targetHost[0],
															ipPayload);

							unsigned char targetGateNet = routerInfo->routeTableGateNet[targetRouteIndex];
							unsigned char targetGateHost = routerInfo->routeTableGateHost[targetRouteIndex];

							if(targetGateNet == 255)
							{
								targetGateNet = targetNet[0];
								targetGateHost = targetHost[0];
							}

							int targetArpIndex = -1;

							for(int i = 0; i < routerInfo->arpCacheCount; i++)
							{
								if(routerInfo->arpCacheNet[i] == routerInfo->routeTableGateNet[targetRouteIndex] && routerInfo->arpCacheHost[i] == routerInfo->routeTableGateHost[targetRouteIndex])
								{
									targetArpIndex = i;
									break;
								}
							}

							if(targetArpIndex = -1)
							{
								// broadcast the request
								unsigned char message[ETHERNET_PAYLOAD_SIZE];
								message[0] = targetGateNet;
								message[1] = targetGateHost;

								char *packetToSend = CreateEthernetPacket(255,
																		  routerInfo->MACs[interfaceIndex],
																		  (unsigned char)1,
																		  4 + ETHERNET_PAYLOAD_SIZE,
																		  message);

								int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

								while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
									bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

								free(packetToSend);

								arpWaitNet = targetGateNet;
								arpWaitHost = targetGateHost;
								arpWaitHavePacket = 1;
								arpWaitIPPacket = ipPacket;
								arpWaitSourceMAC = sourceMAC;
								arpWaitProtocol = (unsigned char)TR_PROTOCOL;
								arpWaitFinalNet = targetNet[0];
								arpWaitFinalHost = targetHost[0];
								arpWaitSendInterface = sendInterface;
								arpShouldPrint = 0;
								expectingARPReply = 1;
								gettimeofday(&arpStartTime, NULL);
							}
							else
							{
								char *packetToSend = CreateEthernetPacket(routerInfo->arpCacheMAC[targetArpIndex],
																		  sourceMAC,
																		  (unsigned char)PING_PROTOCOL,
																		  4 + MAX_IP_PACKET_SIZE,
																		  ipPacket);

								int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

								while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
									bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

								free(ipPacket);
								free(packetToSend);
							}
						}
					}
				}
				else if(buffer[2] == TCP_TEST_PROTOCOL || buffer[2] == TCP_CONN_PROTOCOL || buffer[2] == TCP_SEND_PROTOCOL || buffer[2] == TCP_DONE_PROTOCOL)
				{
					char *ipPacket = payload;
					char *tcpPacket = GetIPPayload(ipPacket);

					unsigned char targetNetIP = ipPacket[5];
					unsigned char targetHostIP = ipPacket[6];

					int targetRouteIndex = -1;

					for(int i = 0; i < routerInfo->routeTableCount; i++)
					{
						if(routerInfo->routeTableDestNet[i] == targetNetIP || routerInfo->routeTableDestNet[i] == DEF_ROUTE)
						{
							targetRouteIndex = i;
							break;
						}
					}

					if(targetRouteIndex != -1)
					{
						// find the right mac
						unsigned char sourceMAC = routerInfo->routeTableSourceMAC[targetRouteIndex];
						int interfaceIndex = 0;
						int sendInterface;

						while(interfaceIndex < 6 && routerInfo->interfaces[interfaceIndex])
						{
							if(routerInfo->MACs[interfaceIndex] == sourceMAC)
							{
								sendInterface = routerInfo->interfaces[interfaceIndex];
								break;
							}

							interfaceIndex++;
						}

						char *newIPPacket = CreateIPPacket(7 + MAX_TCP_PACKET_SIZE,
														   6,
														   buffer[2],
														   ipPacket[3],
														   ipPacket[4],
														   targetNetIP,
														   targetHostIP,
														   tcpPacket);

						unsigned char targetGateNet = routerInfo->routeTableGateNet[targetRouteIndex];
						unsigned char targetGateHost = routerInfo->routeTableGateHost[targetRouteIndex];

						if(targetGateNet == 255)
						{
							targetGateNet = targetNetIP;
							targetGateHost = targetHostIP;
						}

						int targetARPIndex = -1;

						for(int i = 0; i < routerInfo->arpCacheCount; i++)
						{
							if(routerInfo->arpCacheNet[i] == targetGateNet && routerInfo->arpCacheHost[i] == targetGateHost)
							{
								targetARPIndex = i;
								break;
							}
						}

						if(targetARPIndex == -1)
						{
							// broadcast the request
							unsigned char message[ETHERNET_PAYLOAD_SIZE];
							message[0] = targetGateNet;
							message[1] = targetGateHost;

							char *packetToSend = CreateEthernetPacket(255,
																	  sourceMAC,
																	  (unsigned char)1,
																	  4 + ETHERNET_PAYLOAD_SIZE,
																	  message);

							int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

							while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
								bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

							arpWaitNet = targetGateNet;
							arpWaitHost = targetGateHost;
							arpWaitHavePacket = 1;
							arpWaitIPPacket = newIPPacket;
							arpWaitSourceMAC = sourceMAC;
							arpWaitProtocol = (unsigned char)buffer[2];
							arpWaitFinalNet = targetNetIP;
							arpWaitFinalHost = targetHostIP;
							arpWaitSendInterface = sendInterface;
							arpShouldPrint = 0;
							expectingARPReply = 1;
							gettimeofday(&arpStartTime, NULL);

							free(packetToSend);
						}
						else
						{
							char *packetToSend = CreateEthernetPacket(routerInfo->arpCacheMAC[targetARPIndex],
																	  sourceMAC,
																	  (unsigned char)buffer[2],
																	  4 + MAX_IP_PACKET_SIZE,
																	  ipPacket);

							int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

							while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
								bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

							free(packetToSend);
							free(ipPacket);

						}
					}
				}

				// TODO: free(payload);
			}

			interfaceIndex++;
		}

		if(!fdWasSet && FD_ISSET(routerInfo->interpreterFD, &readFDs))
		{
			char charBuffer[1];

			read(routerInfo->interpreterFD, (void *)&buffer, 1);

			// (2) for macsend, (1) for done
			if(buffer[0] == FINISHED_FLAG)
			{
				done = 1;
			}
			else if(buffer[0] == MACSEND_FLAG)
			{
				char receiver[1];
				char sender[1];
				char message[100];

				read(routerInfo->interpreterFD, (void *)&receiver, 1);
				read(routerInfo->interpreterFD, (void *)&sender, 1);

				bytesRead = read(routerInfo->interpreterFD, (void *)&message, 100);

				while(bytesRead < 100)
					bytesRead += read(routerInfo->interpreterFD, (void *)&message + bytesRead, 100 - bytesRead);


				// send the macsend
				int interfaceIndex = 0;
				int sendInterface;

				while(interfaceIndex < 6 && routerInfo->interfaces[interfaceIndex])
				{
					if(routerInfo->MACs[interfaceIndex] == (int)sender[0])
					{
						sendInterface = routerInfo->interfaces[interfaceIndex];
						break;
					}

					interfaceIndex++;
				}

				char *packetToSend = CreateEthernetPacket(receiver[0],
														  sender[0],
														  (unsigned char)0,
														  4 + ETHERNET_PAYLOAD_SIZE,
														  message);

				if(receiver[0] != 255)
				{
					char *payload = GetPayload(packetToSend);
					printf("%s: macsend to %d on %d: %s\n", routerInfo->name, GetEthernetPacketDestMAC(packetToSend), (int)sender[0], payload);
					free(payload);
				}
				else
				{
					char *payload = GetPayload(packetToSend);
					printf("%s: macsend bcast on %d: %s\n", routerInfo->name, (int)sender[0], payload);
					free(payload);
				}

				int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

				while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
					bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

				free(packetToSend);
			}
			else if(buffer[0] == ARPTEST_FLAG)
			{
				unsigned char netIP[1];
				unsigned char hostIP[1];
				unsigned char message[100] = { 0 };

				read(routerInfo->interpreterFD, (void *)&netIP, 1);
				read(routerInfo->interpreterFD, (void *)&hostIP, 1);

				// broadcast the request
				int interfaceIndex = 0;
				int sendInterface = 0;

				while(interfaceIndex < 6 && routerInfo->interfaces[interfaceIndex])
				{
					if(routerInfo->netIPs[interfaceIndex] == netIP[0])
					{
						sendInterface = routerInfo->interfaces[interfaceIndex];
						break;
					}

					interfaceIndex++;
				}

				if(!sendInterface)
				{
					interfaceIndex = 0;
					sendInterface = routerInfo->interfaces[0];
				}

				// generate the message (the ip we're interested in...)
				message[0] = netIP[0];
				message[1] = hostIP[0];

				char *packetToSend = CreateEthernetPacket(255,
														  routerInfo->MACs[interfaceIndex],
														  (unsigned char)1,
														  4 + ETHERNET_PAYLOAD_SIZE,
														  message);

				// printf("%s: arpreq on %d: %d.%d\n", routerInfo->name, routerInfo->MACs[interfaceIndex], netIP[0], hostIP[0]);

				int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

				while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
					bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

				free(packetToSend);

				arpWaitNet = netIP[0];
				arpWaitHost = hostIP[0];
				expectingARPReply = 1;
				gettimeofday(&arpStartTime, NULL);
			}
			else if(buffer[0] == ARPPRT_FLAG)
			{
				if(routerInfo->arpCacheCount == 0)
				{
					printf("%s: ARP cache is currently empty\n", routerInfo->name);
				}
				else if(routerInfo->arpCacheIndex == 0)
				{
					for(int i = 0; i < ARP_CACHE_SIZE; i++)
					{
						printf("%s: IP: %d.%d\tMAC: %d\n", routerInfo->name, routerInfo->arpCacheNet[i], routerInfo->arpCacheHost[i], routerInfo->arpCacheMAC[i]);
					}
				}
				else
				{
					for(int i = 0; i < routerInfo->arpCacheIndex; i++)
					{
						printf("%s: IP: %d.%d\tMAC: %d\n", routerInfo->name, routerInfo->arpCacheNet[i], routerInfo->arpCacheHost[i], routerInfo->arpCacheMAC[i]);
					}
				}
			}
			else if(buffer[0] == ROUTE_FLAG)
			{
				unsigned char charBuffer[1];

				read(routerInfo->interpreterFD, (void *)&charBuffer[0], 1);
				routerInfo->routeTableDestNet[routerInfo->routeTableCount] = charBuffer[0];

				read(routerInfo->interpreterFD, (void *)&charBuffer[0], 1);
				routerInfo->routeTableSourceMAC[routerInfo->routeTableCount] = charBuffer[0];

				read(routerInfo->interpreterFD, (void *)&charBuffer[0], 1);
				routerInfo->routeTableGateNet[routerInfo->routeTableCount] = charBuffer[0];

				read(routerInfo->interpreterFD, (void *)&charBuffer[0], 1);
				routerInfo->routeTableGateHost[routerInfo->routeTableCount] = charBuffer[0];

				routerInfo->routeTableCount++;
			}
			else if(buffer[0] == PING_FLAG)
			{
				unsigned char targetNet[1];
				unsigned char targetHost[1];

				read(routerInfo->interpreterFD, (void *)&targetNet[0], 1);
				read(routerInfo->interpreterFD, (void *)&targetHost[0], 1);

				int targetRouteIndex = -1;

				for(int i = 0; i < routerInfo->routeTableCount; i++)
				{
					if(routerInfo->routeTableDestNet[i] == targetNet[0] || routerInfo->routeTableDestNet[i] == DEF_ROUTE)
					{
						targetRouteIndex = i;
						break;
					}
				}

				if(targetRouteIndex == -1)
				{
					printf("%s: **** no route to IP: %d.%d\n", routerInfo->name, targetNet[0], targetHost[0]);
				}
				else
				{
					// find the right mac
					unsigned char sourceMAC = routerInfo->routeTableSourceMAC[targetRouteIndex];
					int interfaceIndex = 0;
					int sendInterface;

					while(interfaceIndex < 6 && routerInfo->interfaces[interfaceIndex])
					{
						if(routerInfo->MACs[interfaceIndex] == sourceMAC)
						{
							sendInterface = routerInfo->interfaces[interfaceIndex];
							break;
						}

						interfaceIndex++;
					}
	
					char *ipPayload = "";
					char *ipPacket = CreateIPPacket(7 + IP_PAYLOAD_SIZE,
													6,
													PING_PROTOCOL,
													routerInfo->netIPs[interfaceIndex],
													routerInfo->hostIPs[interfaceIndex],
													targetNet[0],
													targetHost[0],
													ipPayload);

					unsigned char targetGateNet = routerInfo->routeTableGateNet[targetRouteIndex];
					unsigned char targetGateHost = routerInfo->routeTableGateHost[targetRouteIndex];

					if(targetGateNet == 255)
					{
						targetGateNet = targetNet[0];
						targetGateHost = targetHost[0];
					}

					int targetArpIndex = -1;

					for(int i = 0; i < routerInfo->arpCacheCount; i++)
					{
						if(routerInfo->arpCacheNet[i] == routerInfo->routeTableGateNet[targetRouteIndex] && routerInfo->arpCacheHost[i] == routerInfo->routeTableGateHost[targetRouteIndex])
						{
							targetArpIndex = i;
							break;
						}
					}

					if(targetArpIndex = -1)
					{
						// broadcast the request
						unsigned char message[ETHERNET_PAYLOAD_SIZE];
						message[0] = targetGateNet;
						message[1] = targetGateHost;

						char *packetToSend = CreateEthernetPacket(255,
																  routerInfo->MACs[interfaceIndex],
																  (unsigned char)1,
																  4 + ETHERNET_PAYLOAD_SIZE,
																  message);

						int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

						while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
							bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

						free(packetToSend);

						arpWaitNet = targetGateNet;
						arpWaitHost = targetGateHost;
						arpWaitHavePacket = 1;
						arpWaitIPPacket = ipPacket;
						arpWaitSourceMAC = sourceMAC;
						arpWaitProtocol = (unsigned char)PING_PROTOCOL;
						arpWaitFinalNet = targetNet[0];
						arpWaitFinalHost = targetHost[0];
						arpWaitSendInterface = sendInterface;
						arpShouldPrint = 1;
						expectingARPReply = 1;
						gettimeofday(&arpStartTime, NULL);
					}
					else
					{
						char *packetToSend = CreateEthernetPacket(routerInfo->arpCacheMAC[targetArpIndex],
																  sourceMAC,
																  (unsigned char)PING_PROTOCOL,
																  4 + MAX_IP_PACKET_SIZE,
																  ipPacket);

						int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

						while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
							bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

						printf("%s: sent ping to %d.%d\n", routerInfo->name, targetNet[0], targetHost[0]);

						free(ipPacket);
						free(packetToSend);
					}
				}
			}
			else if(buffer[0] == TR_FLAG)
			{
				unsigned char targetNet[1];
				unsigned char targetHost[1];

				read(routerInfo->interpreterFD, (void *)&targetNet[0], 1);
				read(routerInfo->interpreterFD, (void *)&targetHost[0], 1);

				int targetRouteIndex = -1;

				for(int i = 0; i < routerInfo->routeTableCount; i++)
				{
					if(routerInfo->routeTableDestNet[i] == targetNet[0] || routerInfo->routeTableDestNet[i] == DEF_ROUTE)
					{
						targetRouteIndex = i;
						break;
					}
				}

				if(targetRouteIndex == -1)
				{
					printf("%s: **** no route to IP: %d.%d\n", routerInfo->name, targetNet[0], targetHost[0]);
				}
				else
				{
					// find the right mac
					unsigned char sourceMAC = routerInfo->routeTableSourceMAC[targetRouteIndex];
					int interfaceIndex = 0;
					int sendInterface;

					while(interfaceIndex < 6 && routerInfo->interfaces[interfaceIndex])
					{
						if(routerInfo->MACs[interfaceIndex] == sourceMAC)
						{
							sendInterface = routerInfo->interfaces[interfaceIndex];
							break;
						}

						interfaceIndex++;
					}
	
					char *ipPayload = "";
					char *ipPacket = CreateIPPacket(7 + IP_PAYLOAD_SIZE,
													6,
													PING_PROTOCOL,
													routerInfo->netIPs[interfaceIndex],
													routerInfo->hostIPs[interfaceIndex],
													targetNet[0],
													targetHost[0],
													ipPayload);

					unsigned char targetGateNet = routerInfo->routeTableGateNet[targetRouteIndex];
					unsigned char targetGateHost = routerInfo->routeTableGateHost[targetRouteIndex];

					if(targetGateNet == 255)
					{
						targetGateNet = targetNet[0];
						targetGateHost = targetHost[0];
					}

					int targetArpIndex = -1;

					for(int i = 0; i < routerInfo->arpCacheCount; i++)
					{
						if(routerInfo->arpCacheNet[i] == routerInfo->routeTableGateNet[targetRouteIndex] && routerInfo->arpCacheHost[i] == routerInfo->routeTableGateHost[targetRouteIndex])
						{
							targetArpIndex = i;
							break;
						}
					}

					if(targetArpIndex = -1)
					{
						// broadcast the request
						unsigned char message[ETHERNET_PAYLOAD_SIZE];
						message[0] = targetGateNet;
						message[1] = targetGateHost;

						char *packetToSend = CreateEthernetPacket(255,
																  routerInfo->MACs[interfaceIndex],
																  (unsigned char)1,
																  4 + ETHERNET_PAYLOAD_SIZE,
																  message);

						int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

						while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
							bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

						free(packetToSend);

						arpWaitNet = targetGateNet;
						arpWaitHost = targetGateHost;
						arpWaitHavePacket = 1;
						arpWaitIPPacket = ipPacket;
						arpWaitSourceMAC = sourceMAC;
						arpWaitProtocol = (unsigned char)PING_PROTOCOL;
						arpWaitFinalNet = targetNet[0];
						arpWaitFinalHost = targetHost[0];
						arpWaitSendInterface = sendInterface;
						arpShouldPrint = 1;
						expectingARPReply = 1;
						gettimeofday(&arpStartTime, NULL);
					}
					else
					{
						char *packetToSend = CreateEthernetPacket(routerInfo->arpCacheMAC[targetArpIndex],
																  sourceMAC,
																  (unsigned char)PING_PROTOCOL,
																  4 + MAX_IP_PACKET_SIZE,
																  ipPacket);

						int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

						while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
							bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

						printf("%s: sent traceroute to %d.%d\n", routerInfo->name, targetNet[0], targetHost[0]);

						free(ipPacket);
						free(packetToSend);
					}
				}
			}	
		}
	}
}

void ActAsHost(HostInfo *hostInfo)
{
	#if DEBUG
	int debug = 1;
	while(debug)
		;
	#endif

	int done = 0;
	int doneWithInterface = 0;
	int rc;
	int bytesRead;
	int setSize = 0;
	fd_set readFDs;
	unsigned char buffer[MAX_ETHERNET_PACKET_SIZE];

	int expectingARPReply = 0;
	struct timeval arpStartTime;
	struct timeval oneSec;
	oneSec.tv_usec = 1000;
	struct timeval timePassed;
	unsigned char arpWaitNet;
	unsigned char arpWaitHost;
	int arpWaitHavePacket;
	unsigned char arpWaitSourceMAC;
	unsigned char arpWaitProtocol;
	int arpWaitFinalHost;
	int arpWaitFinalNet;
	int arpWaitSendInterface;
	int arpShouldPrint;
	unsigned char *arpWaitIPPacket;

	// tcp data
	int activePorts[MAX_ACTIVE_PORTS] = { 0 };
	char tcpMessages[MAX_ACTIVE_PORTS][MAX_TCP_MESSAGES][TCP_PAYLOAD_SIZE];
	int tcpNumMessages = 0;

	while(!done)
	{
		FD_ZERO(&readFDs);

		if(!doneWithInterface)
		{
			FD_SET(hostInfo->interface, &readFDs);
			setSize = hostInfo->interface;
		}

		FD_SET(hostInfo->interpreterFD, &readFDs);

		if(setSize < hostInfo->interpreterFD)
			setSize = hostInfo->interpreterFD;

		setSize++;

		if(!expectingARPReply)
		{
			rc = select(setSize, &readFDs, NULL, NULL, NULL);
		}
		else
		{
			gettimeofday(&timePassed, NULL);
			timePassed.tv_sec -= arpStartTime.tv_sec;
			timePassed.tv_usec -= arpStartTime.tv_usec;

			if(timePassed.tv_sec >= 1 || timePassed.tv_usec > 1000)
			{
				// printf("%s: arpreq to %d.%d timed out\n", hostInfo->name, arpWaitNet, arpWaitHost);
				expectingARPReply = 0;
				arpWaitHavePacket = 0;
				free(arpWaitIPPacket);
				rc = select(setSize, &readFDs, NULL, NULL, NULL);
			}
			else
			{
				struct timeval tv;
				tv.tv_usec = oneSec.tv_usec - timePassed.tv_usec;

				rc = select(setSize, &readFDs, NULL, NULL, &tv);
			}
		}

		if(rc == -1 && errno == EINTR)
			continue;

		if(FD_ISSET(hostInfo->interface, &readFDs))
		{
			bytesRead = read(hostInfo->interface, (void *)&buffer, MAX_ETHERNET_PACKET_SIZE);

			if(bytesRead)
			{
				while(bytesRead < MAX_ETHERNET_PACKET_SIZE)
					bytesRead += read(hostInfo->interface, (void *)&buffer + bytesRead, MAX_ETHERNET_PACKET_SIZE - bytesRead);

					char *payload = GetPayload((char *)buffer);
					if(buffer[2] == MACSEND_PROTOCOL)
						printf("%s: macsend from %d on %d: %s\n", hostInfo->name, GetEthernetPacketSourceMAC(buffer), (int)hostInfo->MAC, payload);
					else if(buffer[2] == ARPREQ_PROTOCOL)
					{
						if(payload[0] == hostInfo->netIP && payload[1] == hostInfo->hostIP)
						{
							// printf("%s: arpreq from %d on %d: %d.%d\n", hostInfo->name, GetEthernetPacketSourceMAC(buffer), (int)hostInfo->MAC, payload[0], payload[1]);

							unsigned char message[100] = { 0 };
							message[0] = payload[0];
							message[1] = payload[1];
							message[2] = hostInfo->MAC;

							char *packetToSend = CreateEthernetPacket(GetEthernetPacketSourceMAC(buffer),
																	  hostInfo->MAC,
																	  (unsigned char)2,
																	  4 + ETHERNET_PAYLOAD_SIZE,
																	  message);
							int bytesWritten = write(hostInfo->interface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

							while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
								bytesWritten += write(hostInfo->interface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

							// printf("%s: arpreply to %d on %d: %d.%d %d\n", hostInfo->name, GetEthernetPacketSourceMAC(buffer), hostInfo->MAC, message[0], message[1], message[2]);
						}
					}
					else if(buffer[2] == ARPREPLY_PROTOCOL)
					{
						// printf("%s: arpreply from %d on %d: %d.%d %d\n", hostInfo->name, GetEthernetPacketSourceMAC(buffer), hostInfo->MAC, payload[0], payload[1], payload[2]);

						// stick the correct info in cache
						int index = hostInfo->arpCacheIndex;
						hostInfo->arpCacheNet[index] = payload[0];
						hostInfo->arpCacheHost[index] = payload[1];
						hostInfo->arpCacheMAC[index] = payload[2];

						hostInfo->arpCacheIndex++;

						if(hostInfo->arpCacheCount != ARP_CACHE_SIZE)
							hostInfo->arpCacheCount++;

						if(hostInfo->arpCacheIndex == ARP_CACHE_SIZE)
							hostInfo->arpCacheIndex = 0;

						if(arpWaitHavePacket)
						{
							char *packetToSend = CreateEthernetPacket(hostInfo->arpCacheMAC[index],
																	  arpWaitSourceMAC,
																	  arpWaitProtocol,
																	  4 + ETHERNET_PAYLOAD_SIZE,
																	  arpWaitIPPacket);

							int bytesWritten = write(arpWaitSendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

							while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
								bytesWritten += write(arpWaitSendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

							if(arpShouldPrint)
							{
								if(arpWaitProtocol == PING_PROTOCOL)
									printf("%s: sent ping to %d.%d\n", hostInfo->name, arpWaitFinalNet, arpWaitFinalHost);
								else if(arpWaitProtocol == TR_PROTOCOL)
									printf("%s: sent traceroute to %d.%d\n", hostInfo->name, arpWaitFinalNet, arpWaitFinalHost);
							}

							free(arpWaitIPPacket);
							free(packetToSend);
						}

						expectingARPReply = 0;
						arpWaitHavePacket = 0;
					}
					else if(buffer[2] == PING_PROTOCOL)
					{
						int forMe = 0;

						if(payload[5] == hostInfo->netIP && payload[6] == hostInfo->hostIP)
							forMe = 1;

						if(forMe)
						{
							printf("%s: received ping from %d.%d\n", hostInfo->name, payload[3], payload[4]);
						}
						else
						{
							unsigned char targetNet[1];
							unsigned char targetHost[1];

							targetNet[0] = payload[5];
							targetHost[0] = payload[6];
							int targetRouteIndex = -1;

							for(int i = 0; i < hostInfo->routeTableCount; i++)
							{
								if(hostInfo->routeTableDestNet[i] == targetNet[0] || hostInfo->routeTableDestNet[i] == DEF_ROUTE)
								{
									targetRouteIndex = i;
									break;
								}
							}

							if(targetRouteIndex == -1)
							{
								printf("%s: **** no route to IP: %d.%d\n", hostInfo->name, targetNet[0], targetHost[0]);
							}
							else
							{
								unsigned char sourceMAC = hostInfo->routeTableSourceMAC[targetRouteIndex];
								int sendInterface = hostInfo->interface;

								char *ipPayload = "";
								char *ipPacket = CreateIPPacket(7 + IP_PAYLOAD_SIZE,
																6,
																PING_PROTOCOL,
																payload[3],
																payload[4],
																targetNet[0],
																targetHost[0],
																ipPayload);

								unsigned char targetGateNet = hostInfo->routeTableGateNet[targetRouteIndex];
								unsigned char targetGateHost = hostInfo->routeTableGateHost[targetRouteIndex];

								if(targetGateNet == 255)
								{
									targetGateNet = targetNet[0];
									targetGateHost = targetHost[0];
								}

								int targetArpIndex = -1;

								for(int i = 0; i < hostInfo->arpCacheCount; i++)
								{
									if(hostInfo->arpCacheNet[i] == hostInfo->routeTableGateNet[targetRouteIndex] && hostInfo->arpCacheHost[i] == hostInfo->routeTableGateHost[targetRouteIndex])
									{
										targetArpIndex = i;
										break;
									}
								}

								if(targetArpIndex = -1)
								{
									// broadcast the request
									unsigned char message[ETHERNET_PAYLOAD_SIZE];
									message[0] = targetGateNet;
									message[1] = targetGateHost;

									char *packetToSend = CreateEthernetPacket(255,
																			  hostInfo->MAC,
																			  (unsigned char)1,
																			  4 + ETHERNET_PAYLOAD_SIZE,
																			  message);

									int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

									while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
										bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

									free(packetToSend);

									arpWaitNet = targetGateNet;
									arpWaitHost = targetGateHost;
									arpWaitHavePacket = 1;
									arpWaitIPPacket = ipPacket;
									arpWaitSourceMAC = sourceMAC;
									arpWaitProtocol = (unsigned char)PING_PROTOCOL;
									arpWaitFinalNet = targetNet[0];
									arpWaitFinalHost = targetHost[0];
									arpWaitSendInterface = sendInterface;
									arpShouldPrint = 0;
									expectingARPReply = 1;
									gettimeofday(&arpStartTime, NULL);
								}
								else
								{
									char *packetToSend = CreateEthernetPacket(hostInfo->arpCacheMAC[targetArpIndex],
																		  sourceMAC,
																		  (unsigned char)PING_PROTOCOL,
																		  4 + MAX_IP_PACKET_SIZE,
																		  ipPacket);

									int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

									while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
										bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

									free(ipPacket);
									free(packetToSend);
								}
							}
						}
					}
					else if(buffer[2] == TR_PROTOCOL)
					{
						int forMe = 0;

						if(payload[5] == hostInfo->netIP && payload[6] == hostInfo->hostIP)
							forMe = 1;

						if(forMe)
						{
							printf("%s: received traceroute from %d.%d\nroute: %s\n", hostInfo->name, payload[3], payload[4], payload[7]);
						}
						else
						{
							unsigned char targetNet[1];
							unsigned char targetHost[1];

							int startOfNewNode = 7 + (3 * (6 - payload[1]));
							payload[startOfNewNode] = hostInfo->name[0];
							payload[startOfNewNode + 1] = hostInfo->name[1];
							payload[startOfNewNode + 2] = ' ';
							payload[1] = payload[1] - 1;

							targetNet[0] = payload[5];
							targetHost[0] = payload[6];
							int targetRouteIndex = -1;

							for(int i = 0; i < hostInfo->routeTableCount; i++)
							{
								if(hostInfo->routeTableDestNet[i] == targetNet[0] || hostInfo->routeTableDestNet[i] == DEF_ROUTE)
								{
									targetRouteIndex = i;
									break;
								}
							}

							if(targetRouteIndex == -1)
							{
								printf("%s: **** no route to IP: %d.%d\n", hostInfo->name, targetNet[0], targetHost[0]);
							}
							else
							{
								unsigned char sourceMAC = hostInfo->routeTableSourceMAC[targetRouteIndex];
								int sendInterface = hostInfo->interface;

								char *ipPayload = calloc(MAX_IP_PACKET_SIZE - 7, sizeof(char));
								strncpy(ipPayload, &payload[7], MAX_IP_PACKET_SIZE - 7);
								char *ipPacket = CreateIPPacket(7 + IP_PAYLOAD_SIZE,
																6,
																TR_PROTOCOL,
																payload[3],
																payload[4],
																targetNet[0],
																targetHost[0],
																ipPayload);

								unsigned char targetGateNet = hostInfo->routeTableGateNet[targetRouteIndex];
								unsigned char targetGateHost = hostInfo->routeTableGateHost[targetRouteIndex];

								if(targetGateNet == 255)
								{
									targetGateNet = targetNet[0];
									targetGateHost = targetHost[0];
								}

								int targetArpIndex = -1;

								for(int i = 0; i < hostInfo->arpCacheCount; i++)
								{
									if(hostInfo->arpCacheNet[i] == hostInfo->routeTableGateNet[targetRouteIndex] && hostInfo->arpCacheHost[i] == hostInfo->routeTableGateHost[targetRouteIndex])
									{
										targetArpIndex = i;
										break;
									}
								}

								if(targetArpIndex = -1)
								{
									// broadcast the request
									unsigned char message[ETHERNET_PAYLOAD_SIZE];
									message[0] = targetGateNet;
									message[1] = targetGateHost;

									char *packetToSend = CreateEthernetPacket(255,
																			  hostInfo->MAC,
																			  (unsigned char)1,
																			  4 + ETHERNET_PAYLOAD_SIZE,
																			  message);

									int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

									while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
										bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

									free(packetToSend);

									arpWaitNet = targetGateNet;
									arpWaitHost = targetGateHost;
									arpWaitHavePacket = 1;
									arpWaitIPPacket = ipPacket;
									arpWaitSourceMAC = sourceMAC;
									arpWaitProtocol = (unsigned char)TR_PROTOCOL;
									arpWaitFinalNet = targetNet[0];
									arpWaitFinalHost = targetHost[0];
									arpWaitSendInterface = sendInterface;
									arpShouldPrint = 0;
									expectingARPReply = 1;
									gettimeofday(&arpStartTime, NULL);
								}
								else
								{
									char *packetToSend = CreateEthernetPacket(hostInfo->arpCacheMAC[targetArpIndex],
																		  sourceMAC,
																		  (unsigned char)PING_PROTOCOL,
																		  4 + MAX_IP_PACKET_SIZE,
																		  ipPacket);

									int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

									while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
										bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

									free(ipPacket);
									free(packetToSend);
								}
							}
						}
					}
					else if(buffer[2] == TCP_TEST_PROTOCOL)
					{
						int forMe = 0;

						char *ipPacket = payload;
						char *tcpPacket = GetIPPayload(ipPacket);

						if(ipPacket[5] == hostInfo->netIP && payload[6] == hostInfo->hostIP)
							forMe = 1;

						if(forMe)
						{
							int portNumber;
							memcpy((void *)&portNumber, (void *)&tcpPacket[0], sizeof(int));
							char *tcpPayload = GetTCPPayload(tcpPacket);
							printf("%s received tcptest from %d.%d for port %d: %s", hostInfo->name, ipPacket[3], ipPacket[4], portNumber, tcpPayload);

							free(tcpPayload);
							free(tcpPacket);
						}
						else
						{
							unsigned char targetNetIP = ipPacket[5];
							unsigned char targetHostIP = ipPacket[6];

							int targetRouteIndex = -1;

							for(int i = 0; i < hostInfo->routeTableCount; i++)
							{
								if(hostInfo->routeTableDestNet[i] == targetNetIP || hostInfo->routeTableDestNet[i] == DEF_ROUTE)
								{
									targetRouteIndex = i;
									break;
								}
							}

							if(targetRouteIndex != -1)
							{
								// find the right mac
								unsigned char sourceMAC = hostInfo->routeTableSourceMAC[targetRouteIndex];
								int sendInterface = hostInfo->interface;

								char *newIPPacket = CreateIPPacket(7 + MAX_TCP_PACKET_SIZE,
																6,
																TCP_TEST_PROTOCOL,
																ipPacket[3],
																ipPacket[4],
																targetNetIP,
																targetHostIP,
																tcpPacket);

								unsigned char targetGateNet = hostInfo->routeTableGateNet[targetRouteIndex];
								unsigned char targetGateHost = hostInfo->routeTableGateHost[targetRouteIndex];

								if(targetGateNet == 255)
								{
									targetGateNet = targetNetIP;
									targetGateHost = targetHostIP;
								}

								int targetARPIndex = -1;

								for(int i = 0; i < hostInfo->arpCacheCount; i++)
								{
									if(hostInfo->arpCacheNet[i] == targetGateNet && hostInfo->arpCacheHost[i] == targetGateHost)
									{
										targetARPIndex = i;
										break;
									}
								}

								if(targetARPIndex == -1)
								{
									// broadcast the request
									unsigned char message[ETHERNET_PAYLOAD_SIZE];
									message[0] = targetGateNet;
									message[1] = targetGateHost;

									char *packetToSend = CreateEthernetPacket(255,
																			  hostInfo->MAC,
																			  (unsigned char)1,
																			  4 + ETHERNET_PAYLOAD_SIZE,
																			  message);

									int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

									while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
										bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

									arpWaitNet = targetGateNet;
									arpWaitHost = targetGateHost;
									arpWaitHavePacket = 1;
									arpWaitIPPacket = newIPPacket;
									arpWaitSourceMAC = sourceMAC;
									arpWaitProtocol = (unsigned char)TCP_TEST_PROTOCOL;
									arpWaitFinalNet = targetNetIP;
									arpWaitFinalHost = targetHostIP;
									arpWaitSendInterface = sendInterface;
									arpShouldPrint = 0;
									expectingARPReply = 1;
									gettimeofday(&arpStartTime, NULL);

									free(packetToSend);
								}
								else
								{
									char *packetToSend = CreateEthernetPacket(hostInfo->arpCacheMAC[targetARPIndex],
																			  sourceMAC,
																			  (unsigned char)TCP_TEST_PROTOCOL,
																			  4 + MAX_IP_PACKET_SIZE,
																			  ipPacket);

									int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

									while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
										bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

									free(packetToSend);
									free(ipPacket);
								}
							}
						}
					}
					else if(buffer[2] == TCP_CONN_PROTOCOL)
					{
						int forMe = 0;

						char *ipPacket = payload;
						char *tcpPacket = GetIPPayload(ipPacket);

						if(ipPacket[5] == hostInfo->netIP && payload[6] == hostInfo->hostIP)
							forMe = 1;

						if(forMe)
						{
							int portNumber;
							memcpy((void *)&portNumber, (void *)&tcpPacket[0], sizeof(int));
							char *tcpPayload = GetTCPPayload(tcpPacket);

							int portIndex = 0;

							while(portIndex < 4)
							{
								if(activePorts[portIndex] == 0)
								{
									activePorts[portIndex] = portNumber;
									break;
								}

								portIndex++;
							}

							free(tcpPayload);
							free(tcpPacket);
						}
						else
						{
							unsigned char targetNetIP = ipPacket[5];
							unsigned char targetHostIP = ipPacket[6];

							int targetRouteIndex = -1;

							for(int i = 0; i < hostInfo->routeTableCount; i++)
							{
								if(hostInfo->routeTableDestNet[i] == targetNetIP || hostInfo->routeTableDestNet[i] == DEF_ROUTE)
								{
									targetRouteIndex = i;
									break;
								}
							}

							if(targetRouteIndex != -1)
							{
								// find the right mac
								unsigned char sourceMAC = hostInfo->routeTableSourceMAC[targetRouteIndex];
								int sendInterface = hostInfo->interface;

								char *newIPPacket = CreateIPPacket(7 + MAX_TCP_PACKET_SIZE,
																6,
																TCP_TEST_PROTOCOL,
																ipPacket[3],
																ipPacket[4],
																targetNetIP,
																targetHostIP,
																tcpPacket);

								unsigned char targetGateNet = hostInfo->routeTableGateNet[targetRouteIndex];
								unsigned char targetGateHost = hostInfo->routeTableGateHost[targetRouteIndex];

								if(targetGateNet == 255)
								{
									targetGateNet = targetNetIP;
									targetGateHost = targetHostIP;
								}

								int targetARPIndex = -1;

								for(int i = 0; i < hostInfo->arpCacheCount; i++)
								{
									if(hostInfo->arpCacheNet[i] == targetGateNet && hostInfo->arpCacheHost[i] == targetGateHost)
									{
										targetARPIndex = i;
										break;
									}
								}

								if(targetARPIndex == -1)
								{
									// broadcast the request
									unsigned char message[ETHERNET_PAYLOAD_SIZE];
									message[0] = targetGateNet;
									message[1] = targetGateHost;

									char *packetToSend = CreateEthernetPacket(255,
																			  hostInfo->MAC,
																			  (unsigned char)1,
																			  4 + ETHERNET_PAYLOAD_SIZE,
																			  message);

									int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

									while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
										bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

									arpWaitNet = targetGateNet;
									arpWaitHost = targetGateHost;
									arpWaitHavePacket = 1;
									arpWaitIPPacket = newIPPacket;
									arpWaitSourceMAC = sourceMAC;
									arpWaitProtocol = (unsigned char)TCP_TEST_PROTOCOL;
									arpWaitFinalNet = targetNetIP;
									arpWaitFinalHost = targetHostIP;
									arpWaitSendInterface = sendInterface;
									arpShouldPrint = 0;
									expectingARPReply = 1;
									gettimeofday(&arpStartTime, NULL);

									free(packetToSend);
								}
								else
								{
									char *packetToSend = CreateEthernetPacket(hostInfo->arpCacheMAC[targetARPIndex],
																			  sourceMAC,
																			  (unsigned char)TCP_TEST_PROTOCOL,
																			  4 + MAX_IP_PACKET_SIZE,
																			  ipPacket);

									int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

									while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
										bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

									free(packetToSend);
									free(ipPacket);
								}
							}
						}
					}
					else if(buffer[2] == TCP_SEND_PROTOCOL)
					{
						int forMe = 0;

						char *ipPacket = payload;
						char *tcpPacket = GetIPPayload(ipPacket);

						if(ipPacket[5] == hostInfo->netIP && payload[6] == hostInfo->hostIP)
							forMe = 1;

						if(forMe)
						{
							int portNumber;
							memcpy((void *)&portNumber, (void *)&tcpPacket[0], sizeof(int));
							char *tcpPayload = GetTCPPayload(tcpPacket);

							// check if the port is actually being used
							int portIndex = 0;

							while(portIndex < 4)
							{
								if(activePorts[portIndex] == portNumber)
									break;

								portIndex++;
							}

							if(portIndex >= 4)
							{
								printf("%s received FTPSend on unused port %d", hostInfo->name, portNumber);
							}
							else
							{
								strncpy(tcpMessages[portIndex][tcpNumMessages], tcpPayload, strlen(tcpPayload));
								tcpNumMessages++;
							}

							free(tcpPayload);
							free(tcpPacket);
						}
						else
						{
							unsigned char targetNetIP = ipPacket[5];
							unsigned char targetHostIP = ipPacket[6];

							int targetRouteIndex = -1;

							for(int i = 0; i < hostInfo->routeTableCount; i++)
							{
								if(hostInfo->routeTableDestNet[i] == targetNetIP || hostInfo->routeTableDestNet[i] == DEF_ROUTE)
								{
									targetRouteIndex = i;
									break;
								}
							}

							if(targetRouteIndex != -1)
							{
								// find the right mac
								unsigned char sourceMAC = hostInfo->routeTableSourceMAC[targetRouteIndex];
								int sendInterface = hostInfo->interface;

								char *newIPPacket = CreateIPPacket(7 + MAX_TCP_PACKET_SIZE,
																6,
																TCP_TEST_PROTOCOL,
																ipPacket[3],
																ipPacket[4],
																targetNetIP,
																targetHostIP,
																tcpPacket);

								unsigned char targetGateNet = hostInfo->routeTableGateNet[targetRouteIndex];
								unsigned char targetGateHost = hostInfo->routeTableGateHost[targetRouteIndex];

								if(targetGateNet == 255)
								{
									targetGateNet = targetNetIP;
									targetGateHost = targetHostIP;
								}

								int targetARPIndex = -1;

								for(int i = 0; i < hostInfo->arpCacheCount; i++)
								{
									if(hostInfo->arpCacheNet[i] == targetGateNet && hostInfo->arpCacheHost[i] == targetGateHost)
									{
										targetARPIndex = i;
										break;
									}
								}

								if(targetARPIndex == -1)
								{
									// broadcast the request
									unsigned char message[ETHERNET_PAYLOAD_SIZE];
									message[0] = targetGateNet;
									message[1] = targetGateHost;

									char *packetToSend = CreateEthernetPacket(255,
																			  hostInfo->MAC,
																			  (unsigned char)1,
																			  4 + ETHERNET_PAYLOAD_SIZE,
																			  message);

									int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

									while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
										bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

									arpWaitNet = targetGateNet;
									arpWaitHost = targetGateHost;
									arpWaitHavePacket = 1;
									arpWaitIPPacket = newIPPacket;
									arpWaitSourceMAC = sourceMAC;
									arpWaitProtocol = (unsigned char)TCP_TEST_PROTOCOL;
									arpWaitFinalNet = targetNetIP;
									arpWaitFinalHost = targetHostIP;
									arpWaitSendInterface = sendInterface;
									arpShouldPrint = 0;
									expectingARPReply = 1;
									gettimeofday(&arpStartTime, NULL);

									free(packetToSend);
								}
								else
								{
									char *packetToSend = CreateEthernetPacket(hostInfo->arpCacheMAC[targetARPIndex],
																			  sourceMAC,
																			  (unsigned char)TCP_TEST_PROTOCOL,
																			  4 + MAX_IP_PACKET_SIZE,
																			  ipPacket);

									int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

									while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
										bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

									free(packetToSend);
									free(ipPacket);
								}
							}
						}
					}
					else if(buffer[2] == TCP_DONE_PROTOCOL)
					{
						int forMe = 0;

						char *ipPacket = payload;
						char *tcpPacket = GetIPPayload(ipPacket);

						if(ipPacket[5] == hostInfo->netIP && payload[6] == hostInfo->hostIP)
							forMe = 1;

						if(forMe)
						{
							int portNumber;
							memcpy((void *)&portNumber, (void *)&tcpPacket[0], sizeof(int));
							char *tcpPayload = GetTCPPayload(tcpPacket);

							// TODO: what to do for done
							// print out a message

							// get rid of the shit...

							free(tcpPayload);
							free(tcpPacket);
						}
						else
						{
							unsigned char targetNetIP = ipPacket[5];
							unsigned char targetHostIP = ipPacket[6];

							int targetRouteIndex = -1;

							for(int i = 0; i < hostInfo->routeTableCount; i++)
							{
								if(hostInfo->routeTableDestNet[i] == targetNetIP || hostInfo->routeTableDestNet[i] == DEF_ROUTE)
								{
									targetRouteIndex = i;
									break;
								}
							}

							if(targetRouteIndex != -1)
							{
								// find the right mac
								unsigned char sourceMAC = hostInfo->routeTableSourceMAC[targetRouteIndex];
								int sendInterface = hostInfo->interface;

								char *newIPPacket = CreateIPPacket(7 + MAX_TCP_PACKET_SIZE,
																6,
																TCP_TEST_PROTOCOL,
																ipPacket[3],
																ipPacket[4],
																targetNetIP,
																targetHostIP,
																tcpPacket);

								unsigned char targetGateNet = hostInfo->routeTableGateNet[targetRouteIndex];
								unsigned char targetGateHost = hostInfo->routeTableGateHost[targetRouteIndex];

								if(targetGateNet == 255)
								{
									targetGateNet = targetNetIP;
									targetGateHost = targetHostIP;
								}

								int targetARPIndex = -1;

								for(int i = 0; i < hostInfo->arpCacheCount; i++)
								{
									if(hostInfo->arpCacheNet[i] == targetGateNet && hostInfo->arpCacheHost[i] == targetGateHost)
									{
										targetARPIndex = i;
										break;
									}
								}

								if(targetARPIndex == -1)
								{
									// broadcast the request
									unsigned char message[ETHERNET_PAYLOAD_SIZE];
									message[0] = targetGateNet;
									message[1] = targetGateHost;

									char *packetToSend = CreateEthernetPacket(255,
																			  hostInfo->MAC,
																			  (unsigned char)1,
																			  4 + ETHERNET_PAYLOAD_SIZE,
																			  message);

									int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

									while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
										bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

									arpWaitNet = targetGateNet;
									arpWaitHost = targetGateHost;
									arpWaitHavePacket = 1;
									arpWaitIPPacket = newIPPacket;
									arpWaitSourceMAC = sourceMAC;
									arpWaitProtocol = (unsigned char)TCP_TEST_PROTOCOL;
									arpWaitFinalNet = targetNetIP;
									arpWaitFinalHost = targetHostIP;
									arpWaitSendInterface = sendInterface;
									arpShouldPrint = 0;
									expectingARPReply = 1;
									gettimeofday(&arpStartTime, NULL);

									free(packetToSend);
								}
								else
								{
									char *packetToSend = CreateEthernetPacket(hostInfo->arpCacheMAC[targetARPIndex],
																			  sourceMAC,
																			  (unsigned char)TCP_TEST_PROTOCOL,
																			  4 + MAX_IP_PACKET_SIZE,
																			  ipPacket);

									int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

									while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
										bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

									free(packetToSend);
									free(ipPacket);
								}
							}
						}
					}

					free(payload);
			}
			else
			{
				// remove from the set
				doneWithInterface = 1;
			}
		}
		else if(FD_ISSET(hostInfo->interpreterFD, &readFDs))
		{
			char charBuffer[1];

			read(hostInfo->interpreterFD, (void *)&buffer, 1);

			// (2) for macsend, (1) for done
			if(buffer[0] == FINISHED_FLAG)
			{
				done = 1;
			}
			else if(buffer[0] == MACSEND_FLAG)
			{
				unsigned char receiver[1];
				unsigned char sender[1];
				unsigned char message[100];

				read(hostInfo->interpreterFD, (void *)&receiver, 1);
				read(hostInfo->interpreterFD, (void *)&sender, 1);

				bytesRead = read(hostInfo->interpreterFD, (void *)&message, 100);

				while(bytesRead < 100)
					bytesRead += read(hostInfo->interpreterFD, (void *)&message + bytesRead, 100 - bytesRead);

				// send the macsend
				char *packetToSend = CreateEthernetPacket(receiver[0],
														  sender[0],
														  (unsigned char)0,
														  4 + ETHERNET_PAYLOAD_SIZE,
														  message);

				if(receiver[0] != 255)
				{
					char *payload = GetPayload(packetToSend);
					printf("%s: macsend to %d on %d: %s\n", hostInfo->name, GetEthernetPacketDestMAC(packetToSend), (int)sender[0], payload);
					free(payload);
				}
				else
				{
					char *payload = GetPayload(packetToSend);
					printf("%s: macsend bcast on %d: %s\n", hostInfo->name, (int)sender[0], payload);
					free(payload);
				}

				int bytesWritten = write(hostInfo->interface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

				while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
					bytesWritten += write(hostInfo->interface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

				free(packetToSend);
			}
			else if(buffer[0] == ARPTEST_FLAG)
			{
				unsigned char netIP[1];
				unsigned char hostIP[1];
				unsigned char message[100] = { 0 };

				int sendInterface = hostInfo->interface;

				read(hostInfo->interpreterFD, (void *)&netIP, 1);
				read(hostInfo->interpreterFD, (void *)&hostIP, 1);

				// broadcast the request
				message[0] = netIP[0];
				message[1] = hostIP[0];

				char *packetToSend = CreateEthernetPacket(255,
														  hostInfo->MAC,
														  (unsigned char)1,
														  4 + ETHERNET_PAYLOAD_SIZE,
														  message);

				// printf("%s: arpreq on %d: %d.%d\n", hostInfo->name, hostInfo->MAC, netIP[0], hostIP[0]);

				int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

				while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
					bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

				free(packetToSend);

				arpWaitNet = netIP[0];
				arpWaitHost = hostIP[0];
				expectingARPReply = 1;
				gettimeofday(&arpStartTime, NULL);
			}
			else if(buffer[0] == ARPPRT_FLAG)
			{
				if(hostInfo->arpCacheIndex == 0 && hostInfo->arpCacheMAC[0] == 0)
				{
					printf("%s: ARP cache is currently empty\n", hostInfo->name);
				}
				else if(hostInfo->arpCacheIndex == 0)
				{
					for(int i = 0; i < ARP_CACHE_SIZE; i++)
					{
						printf("%s: IP: %d.%d\tMAC: %d\n", hostInfo->name, hostInfo->arpCacheNet[i], hostInfo->arpCacheHost[i], hostInfo->arpCacheMAC[i]);
					}
				}
				else
				{
					for(int i = 0; i < hostInfo->arpCacheIndex; i++)
					{
						printf("%s: IP: %d.%d\tMAC: %d\n", hostInfo->name, hostInfo->arpCacheNet[i], hostInfo->arpCacheHost[i], hostInfo->arpCacheMAC[i]);
					}

				}
			}
			else if(buffer[0] == ROUTE_FLAG)
			{
				unsigned char charBuffer[1];

				read(hostInfo->interpreterFD, (void *)&charBuffer[0], 1);
				hostInfo->routeTableDestNet[hostInfo->routeTableCount] = charBuffer[0];

				read(hostInfo->interpreterFD, (void *)&charBuffer[0], 1);
				hostInfo->routeTableSourceMAC[hostInfo->routeTableCount] = charBuffer[0];
				
				read(hostInfo->interpreterFD, (void *)&charBuffer[0], 1);
				hostInfo->routeTableGateNet[hostInfo->routeTableCount] = charBuffer[0];

				read(hostInfo->interpreterFD, (void *)&charBuffer[0], 1);
				hostInfo->routeTableGateHost[hostInfo->routeTableCount] = charBuffer[0];

				hostInfo->routeTableCount++;
			}
			else if(buffer[0] == PING_FLAG)
			{
				unsigned char targetNet[1];
				unsigned char targetHost[1];

				read(hostInfo->interpreterFD, (void *)&targetNet[0], 1);
				read(hostInfo->interpreterFD, (void *)&targetHost[0], 1);

				int targetRouteIndex = -1;

				for(int i = 0; i < hostInfo->routeTableCount; i++)
				{
					if(hostInfo->routeTableDestNet[i] == targetNet[0] || hostInfo->routeTableDestNet[i] == DEF_ROUTE)
					{
						targetRouteIndex = i;
						break;
					}
				}

				if(targetRouteIndex == -1)
				{
					printf("%s: **** no route to IP: %d.%d\n", hostInfo->name, targetNet[0], targetHost[0]);
				}
				else
				{
					// find the right mac
					unsigned char sourceMAC = hostInfo->routeTableSourceMAC[targetRouteIndex];
					int sendInterface = hostInfo->interface;

					char *ipPayload = "";
					char *ipPacket = CreateIPPacket(7 + IP_PAYLOAD_SIZE,
													6,
													PING_PROTOCOL,
													hostInfo->netIP,
													hostInfo->hostIP,
													targetNet[0],
													targetHost[0],
													ipPayload);

					unsigned char targetGateNet = hostInfo->routeTableGateNet[targetRouteIndex];
					unsigned char targetGateHost = hostInfo->routeTableGateHost[targetRouteIndex];

					if(targetGateNet == 255)
					{
						targetGateNet = targetNet[0];
						targetGateHost = targetHost[0];
					}

					int targetArpIndex = -1;

					for(int i = 0; i < hostInfo->arpCacheCount; i++)
					{
						if(hostInfo->arpCacheNet[i] == hostInfo->routeTableGateNet[targetRouteIndex] && hostInfo->arpCacheHost[i] == hostInfo->routeTableGateHost[targetRouteIndex])
						{
							targetArpIndex = i;
							break;
						}
					}

					if(targetArpIndex = -1)
					{
						// broadcast the request
						unsigned char message[ETHERNET_PAYLOAD_SIZE];
						message[0] = targetGateNet;
						message[1] = targetGateHost;

						char *packetToSend = CreateEthernetPacket(255,
																  hostInfo->MAC,
																  (unsigned char)1,
																  4 + ETHERNET_PAYLOAD_SIZE,
																  message);

						int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

						while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
							bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

						free(packetToSend);

						arpWaitNet = targetGateNet;
						arpWaitHost = targetGateHost;
						arpWaitHavePacket = 1;
						arpWaitIPPacket = ipPacket;
						arpWaitSourceMAC = sourceMAC;
						arpWaitProtocol = (unsigned char)PING_PROTOCOL;
						arpWaitFinalNet = targetNet[0];
						arpWaitFinalHost = targetHost[0];
						arpWaitSendInterface = sendInterface;
						arpShouldPrint = 1;
						expectingARPReply = 1;
						gettimeofday(&arpStartTime, NULL);
					}
					else
					{
						char *packetToSend = CreateEthernetPacket(hostInfo->arpCacheMAC[targetArpIndex],
																  sourceMAC,
																  (unsigned char)PING_PROTOCOL,
																  4 + MAX_IP_PACKET_SIZE,
																  ipPacket);

						int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

						while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
							bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

						printf("%s: sent ping to %d.%d\n", hostInfo->name, targetNet[0], targetHost[0]);

						free(packetToSend);
						free(ipPacket);
					}
				}
			}
			else if(buffer[0] == TR_FLAG)
			{
				unsigned char targetNet[1];
				unsigned char targetHost[1];

				read(hostInfo->interpreterFD, (void *)&targetNet[0], 1);
				read(hostInfo->interpreterFD, (void *)&targetHost[0], 1);

				int targetRouteIndex = -1;

				for(int i = 0; i < hostInfo->routeTableCount; i++)
				{
					if(hostInfo->routeTableDestNet[i] == targetNet[0] || hostInfo->routeTableDestNet[i] == DEF_ROUTE)
					{
						targetRouteIndex = i;
						break;
					}
				}

				if(targetRouteIndex == -1)
				{
					printf("%s: **** no route to IP: %d.%d\n", hostInfo->name, targetNet[0], targetHost[0]);
				}
				else
				{
					// find the right mac
					unsigned char sourceMAC = hostInfo->routeTableSourceMAC[targetRouteIndex];
					int sendInterface = hostInfo->interface;

					char *ipPayload = "";
					char *ipPacket = CreateIPPacket(7 + IP_PAYLOAD_SIZE,
													6,
													TR_PROTOCOL,
													hostInfo->netIP,
													hostInfo->hostIP,
													targetNet[0],
													targetHost[0],
													ipPayload);

					unsigned char targetGateNet = hostInfo->routeTableGateNet[targetRouteIndex];
					unsigned char targetGateHost = hostInfo->routeTableGateHost[targetRouteIndex];

					if(targetGateNet == 255)
					{
						targetGateNet = targetNet[0];
						targetGateHost = targetHost[0];
					}

					int targetArpIndex = -1;

					for(int i = 0; i < hostInfo->arpCacheCount; i++)
					{
						if(hostInfo->arpCacheNet[i] == hostInfo->routeTableGateNet[targetRouteIndex] && hostInfo->arpCacheHost[i] == hostInfo->routeTableGateHost[targetRouteIndex])
						{
							targetArpIndex = i;
							break;
						}
					}

					if(targetArpIndex = -1)
					{
						// broadcast the request
						unsigned char message[ETHERNET_PAYLOAD_SIZE];
						message[0] = targetGateNet;
						message[1] = targetGateHost;

						char *packetToSend = CreateEthernetPacket(255,
																  hostInfo->MAC,
																  (unsigned char)1,
																  4 + ETHERNET_PAYLOAD_SIZE,
																  message);

						int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

						while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
							bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

						free(packetToSend);

						arpWaitNet = targetGateNet;
						arpWaitHost = targetGateHost;
						arpWaitHavePacket = 1;
						arpWaitIPPacket = ipPacket;
						arpWaitSourceMAC = sourceMAC;
						arpWaitProtocol = (unsigned char)TR_PROTOCOL;
						arpWaitFinalNet = targetNet[0];
						arpWaitFinalHost = targetHost[0];
						arpWaitSendInterface = sendInterface;
						arpShouldPrint = 1;
						expectingARPReply = 1;
						gettimeofday(&arpStartTime, NULL);
					}
					else
					{
						char *packetToSend = CreateEthernetPacket(hostInfo->arpCacheMAC[targetArpIndex],
																  sourceMAC,
																  (unsigned char)PING_PROTOCOL,
																  4 + MAX_IP_PACKET_SIZE,
																  ipPacket);

						int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

						while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
							bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

						printf("%s: sent traceroute to %d.%d\n", hostInfo->name, targetNet[0], targetHost[0]);

						free(ipPacket);
						free(packetToSend);
					}
				}
			}
			else if(buffer[0] == TCP_TEST_FLAG)
			{
				unsigned char charBuffer;
				charBuffer = '5';

				write(hostInfo->interpreterFD, (void *)&charBuffer, 1);

				unsigned char targetNetIP;
				unsigned char targetHostIP;
				read(hostInfo->interpreterFD, (void *)&targetNetIP, 1);
				read(hostInfo->interpreterFD, (void *)&targetHostIP, 1);

				int targetPort;
				read(hostInfo->interpreterFD, (void *)&targetPort, sizeof(int));

				unsigned char tcpPayload[TCP_PAYLOAD_SIZE];
				int tcpPayloadBytes = read(hostInfo->interpreterFD, (void *)tcpPayload, sizeof(unsigned char) * TCP_PAYLOAD_SIZE);

				// create the tcp packet
				unsigned char tcpPacket[MAX_TCP_PACKET_SIZE];
				memcpy((void *)&tcpPacket[0], (void *)&targetPort, sizeof(int));		// should be 4 on our machines... not portable

				// to get int back out...
				// memcpy((void *)&something, (void *)&tcpPacket[0], sizeof(int));

				memcpy((void *)&tcpPacket[4], (void *)&tcpPayload, TCP_PAYLOAD_SIZE);

				// create the ip packet
				int targetRouteIndex = -1;
				
				for(int i = 0; i < hostInfo->routeTableCount; i++)
				{
					if(hostInfo->routeTableDestNet[i] == targetNetIP || hostInfo->routeTableDestNet[i] == DEF_ROUTE)
					{
						targetRouteIndex = i;
						break;
					}
				}

				if(targetRouteIndex != -1)
				{
					// find the right mac
					unsigned char sourceMAC = hostInfo->routeTableSourceMAC[targetRouteIndex];
					int sendInterface = hostInfo->interface;

					char *ipPacket = CreateIPPacket(7 + MAX_TCP_PACKET_SIZE,
													6,
													TCP_TEST_PROTOCOL,
													hostInfo->netIP,
													hostInfo->hostIP,
													targetNetIP,
													targetHostIP,
													tcpPacket);

					unsigned char targetGateNet = hostInfo->routeTableGateNet[targetRouteIndex];
					unsigned char targetGateHost = hostInfo->routeTableGateHost[targetRouteIndex];

					if(targetGateNet == 255)
					{
						targetGateNet = targetNetIP;
						targetGateHost = targetHostIP;
					}

					int targetARPIndex = -1;

					for(int i = 0; i < hostInfo->arpCacheCount; i++)
					{
						if(hostInfo->arpCacheNet[i] == targetGateNet && hostInfo->arpCacheHost[i] == targetGateHost)
						{
							targetARPIndex = i;
							break;
						}
					}

					if(targetARPIndex == -1)
					{
						// broadcast the request
						unsigned char message[ETHERNET_PAYLOAD_SIZE];
						message[0] = targetGateNet;
						message[1] = targetGateHost;

						char *packetToSend = CreateEthernetPacket(255,
																  hostInfo->MAC,
																  (unsigned char)1,
																  4 + ETHERNET_PAYLOAD_SIZE,
																  message);

						int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

						while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
							bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

						arpWaitNet = targetGateNet;
						arpWaitHost = targetGateHost;
						arpWaitHavePacket = 1;
						arpWaitIPPacket = ipPacket;
						arpWaitSourceMAC = sourceMAC;
						arpWaitProtocol = (unsigned char)TCP_TEST_PROTOCOL;
						arpWaitFinalNet = targetNetIP;
						arpWaitFinalHost = targetHostIP;
						arpWaitSendInterface = sendInterface;
						arpShouldPrint = 0;
						expectingARPReply = 1;
						gettimeofday(&arpStartTime, NULL);

						free(packetToSend);
					}
					else
					{
						char *packetToSend = CreateEthernetPacket(hostInfo->arpCacheMAC[targetARPIndex],
																  sourceMAC,
																  (unsigned char)TCP_TEST_PROTOCOL,
																  4 + MAX_IP_PACKET_SIZE,
																  ipPacket);

						int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

						while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
							bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

						free(packetToSend);
						free(ipPacket);
					}
				}
			}
			else if(buffer[0] == TCP_CONN_FLAG)
			{
				unsigned char charBuffer;
				charBuffer = '5';

				write(hostInfo->interpreterFD, (void *)&charBuffer, 1);

				unsigned char targetNetIP;
				unsigned char targetHostIP;
				read(hostInfo->interpreterFD, (void *)&targetNetIP, 1);
				read(hostInfo->interpreterFD, (void *)&targetHostIP, 1);

				int targetPort;
				read(hostInfo->interpreterFD, (void *)&targetPort, sizeof(int));

				// create the tcp packet
				unsigned char tcpPacket[MAX_TCP_PACKET_SIZE];
				memcpy((void *)&tcpPacket[0], (void *)&targetPort, sizeof(int));		// should be 4 on our machines... not portable

				// create the ip packet
				int targetRouteIndex = -1;
				
				for(int i = 0; i < hostInfo->routeTableCount; i++)
				{
					if(hostInfo->routeTableDestNet[i] == targetNetIP || hostInfo->routeTableDestNet[i] == DEF_ROUTE)
					{
						targetRouteIndex = i;
						break;
					}
				}

				if(targetRouteIndex != -1)
				{
					// find the right mac
					unsigned char sourceMAC = hostInfo->routeTableSourceMAC[targetRouteIndex];
					int sendInterface = hostInfo->interface;

					char *ipPacket = CreateIPPacket(7 + MAX_TCP_PACKET_SIZE,
													6,
													TCP_CONN_PROTOCOL,
													hostInfo->netIP,
													hostInfo->hostIP,
													targetNetIP,
													targetHostIP,
													tcpPacket);

					unsigned char targetGateNet = hostInfo->routeTableGateNet[targetRouteIndex];
					unsigned char targetGateHost = hostInfo->routeTableGateHost[targetRouteIndex];

					if(targetGateNet == 255)
					{
						targetGateNet = targetNetIP;
						targetGateHost = targetHostIP;
					}

					int targetARPIndex = -1;

					for(int i = 0; i < hostInfo->arpCacheCount; i++)
					{
						if(hostInfo->arpCacheNet[i] == targetGateNet && hostInfo->arpCacheHost[i] == targetGateHost)
						{
							targetARPIndex = i;
							break;
						}
					}

					if(targetARPIndex == -1)
					{
						// broadcast the request
						unsigned char message[ETHERNET_PAYLOAD_SIZE];
						message[0] = targetGateNet;
						message[1] = targetGateHost;

						char *packetToSend = CreateEthernetPacket(255,
																  hostInfo->MAC,
																  (unsigned char)1,
																  4 + ETHERNET_PAYLOAD_SIZE,
																  message);

						int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

						while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
							bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

						arpWaitNet = targetGateNet;
						arpWaitHost = targetGateHost;
						arpWaitHavePacket = 1;
						arpWaitIPPacket = ipPacket;
						arpWaitSourceMAC = sourceMAC;
						arpWaitProtocol = (unsigned char)TCP_CONN_PROTOCOL;
						arpWaitFinalNet = targetNetIP;
						arpWaitFinalHost = targetHostIP;
						arpWaitSendInterface = sendInterface;
						arpShouldPrint = 0;
						expectingARPReply = 1;
						gettimeofday(&arpStartTime, NULL);

						free(packetToSend);
					}
					else
					{
						char *packetToSend = CreateEthernetPacket(hostInfo->arpCacheMAC[targetARPIndex],
																  sourceMAC,
																  (unsigned char)TCP_CONN_PROTOCOL,
																  4 + MAX_IP_PACKET_SIZE,
																  ipPacket);

						int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

						while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
							bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

						free(packetToSend);
						free(ipPacket);
					}
				}
			}
			else if(buffer[0] == TCP_SEND_FLAG)
			{
				unsigned char charBuffer;
				charBuffer = '5';

				write(hostInfo->interpreterFD, (void *)&charBuffer, 1);

				unsigned char targetNetIP;
				unsigned char targetHostIP;
				read(hostInfo->interpreterFD, (void *)&targetNetIP, 1);
				read(hostInfo->interpreterFD, (void *)&targetHostIP, 1);

				int targetPort;
				read(hostInfo->interpreterFD, (void *)&targetPort, sizeof(int));

				unsigned char tcpPayload[TCP_PAYLOAD_SIZE];
				int tcpPayloadBytes = read(hostInfo->interpreterFD, (void *)tcpPayload, sizeof(unsigned char) * TCP_PAYLOAD_SIZE);

				// create the tcp packet
				unsigned char tcpPacket[MAX_TCP_PACKET_SIZE];
				memcpy((void *)&tcpPacket[0], (void *)&targetPort, sizeof(int));		// should be 4 on our machines... not portable

				// to get int back out...
				// memcpy((void *)&something, (void *)&tcpPacket[0], sizeof(int));

				memcpy((void *)&tcpPacket[4], (void *)&tcpPayload, TCP_PAYLOAD_SIZE);

				// create the ip packet
				int targetRouteIndex = -1;
				
				for(int i = 0; i < hostInfo->routeTableCount; i++)
				{
					if(hostInfo->routeTableDestNet[i] == targetNetIP || hostInfo->routeTableDestNet[i] == DEF_ROUTE)
					{
						targetRouteIndex = i;
						break;
					}
				}

				if(targetRouteIndex != -1)
				{
					// find the right mac
					unsigned char sourceMAC = hostInfo->routeTableSourceMAC[targetRouteIndex];
					int sendInterface = hostInfo->interface;

					char *ipPacket = CreateIPPacket(7 + MAX_TCP_PACKET_SIZE,
													6,
													TCP_SEND_PROTOCOL,
													hostInfo->netIP,
													hostInfo->hostIP,
													targetNetIP,
													targetHostIP,
													tcpPacket);

					unsigned char targetGateNet = hostInfo->routeTableGateNet[targetRouteIndex];
					unsigned char targetGateHost = hostInfo->routeTableGateHost[targetRouteIndex];

					if(targetGateNet == 255)
					{
						targetGateNet = targetNetIP;
						targetGateHost = targetHostIP;
					}

					int targetARPIndex = -1;

					for(int i = 0; i < hostInfo->arpCacheCount; i++)
					{
						if(hostInfo->arpCacheNet[i] == targetGateNet && hostInfo->arpCacheHost[i] == targetGateHost)
						{
							targetARPIndex = i;
							break;
						}
					}

					if(targetARPIndex == -1)
					{
						// broadcast the request
						unsigned char message[ETHERNET_PAYLOAD_SIZE];
						message[0] = targetGateNet;
						message[1] = targetGateHost;

						char *packetToSend = CreateEthernetPacket(255,
																  hostInfo->MAC,
																  (unsigned char)1,
																  4 + ETHERNET_PAYLOAD_SIZE,
																  message);

						int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

						while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
							bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

						arpWaitNet = targetGateNet;
						arpWaitHost = targetGateHost;
						arpWaitHavePacket = 1;
						arpWaitIPPacket = ipPacket;
						arpWaitSourceMAC = sourceMAC;
						arpWaitProtocol = (unsigned char)TCP_SEND_PROTOCOL;
						arpWaitFinalNet = targetNetIP;
						arpWaitFinalHost = targetHostIP;
						arpWaitSendInterface = sendInterface;
						arpShouldPrint = 0;
						expectingARPReply = 1;
						gettimeofday(&arpStartTime, NULL);

						free(packetToSend);
					}
					else
					{
						char *packetToSend = CreateEthernetPacket(hostInfo->arpCacheMAC[targetARPIndex],
																  sourceMAC,
																  (unsigned char)TCP_SEND_PROTOCOL,
																  4 + MAX_IP_PACKET_SIZE,
																  ipPacket);

						int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

						while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
							bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

						free(packetToSend);
						free(ipPacket);
					}
				}
			}
			else if(buffer[0] == TCP_DONE_FLAG)
			{
				unsigned char charBuffer;
				charBuffer = '5';

				write(hostInfo->interpreterFD, (void *)&charBuffer, 1);

				unsigned char targetNetIP;
				unsigned char targetHostIP;
				read(hostInfo->interpreterFD, (void *)&targetNetIP, 1);
				read(hostInfo->interpreterFD, (void *)&targetHostIP, 1);

				int targetPort;
				read(hostInfo->interpreterFD, (void *)&targetPort, sizeof(int));

				// create the tcp packet
				unsigned char tcpPacket[MAX_TCP_PACKET_SIZE];
				memcpy((void *)&tcpPacket[0], (void *)&targetPort, sizeof(int));		// should be 4 on our machines... not portable

				// create the ip packet
				int targetRouteIndex = -1;
				
				for(int i = 0; i < hostInfo->routeTableCount; i++)
				{
					if(hostInfo->routeTableDestNet[i] == targetNetIP || hostInfo->routeTableDestNet[i] == DEF_ROUTE)
					{
						targetRouteIndex = i;
						break;
					}
				}

				if(targetRouteIndex != -1)
				{
					// find the right mac
					unsigned char sourceMAC = hostInfo->routeTableSourceMAC[targetRouteIndex];
					int sendInterface = hostInfo->interface;

					char *ipPacket = CreateIPPacket(7 + MAX_TCP_PACKET_SIZE,
													6,
													TCP_DONE_PROTOCOL,
													hostInfo->netIP,
													hostInfo->hostIP,
													targetNetIP,
													targetHostIP,
													tcpPacket);

					unsigned char targetGateNet = hostInfo->routeTableGateNet[targetRouteIndex];
					unsigned char targetGateHost = hostInfo->routeTableGateHost[targetRouteIndex];

					if(targetGateNet == 255)
					{
						targetGateNet = targetNetIP;
						targetGateHost = targetHostIP;
					}

					int targetARPIndex = -1;

					for(int i = 0; i < hostInfo->arpCacheCount; i++)
					{
						if(hostInfo->arpCacheNet[i] == targetGateNet && hostInfo->arpCacheHost[i] == targetGateHost)
						{
							targetARPIndex = i;
							break;
						}
					}

					if(targetARPIndex == -1)
					{
						// broadcast the request
						unsigned char message[ETHERNET_PAYLOAD_SIZE];
						message[0] = targetGateNet;
						message[1] = targetGateHost;

						char *packetToSend = CreateEthernetPacket(255,
																  hostInfo->MAC,
																  (unsigned char)1,
																  4 + ETHERNET_PAYLOAD_SIZE,
																  message);

						int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

						while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
							bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

						arpWaitNet = targetGateNet;
						arpWaitHost = targetGateHost;
						arpWaitHavePacket = 1;
						arpWaitIPPacket = ipPacket;
						arpWaitSourceMAC = sourceMAC;
						arpWaitProtocol = (unsigned char)TCP_DONE_PROTOCOL;
						arpWaitFinalNet = targetNetIP;
						arpWaitFinalHost = targetHostIP;
						arpWaitSendInterface = sendInterface;
						arpShouldPrint = 0;
						expectingARPReply = 1;
						gettimeofday(&arpStartTime, NULL);

						free(packetToSend);
					}
					else
					{
						char *packetToSend = CreateEthernetPacket(hostInfo->arpCacheMAC[targetARPIndex],
																  sourceMAC,
																  (unsigned char)TCP_DONE_PROTOCOL,
																  4 + MAX_IP_PACKET_SIZE,
																  ipPacket);

						int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

						while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
							bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

						free(packetToSend);
						free(ipPacket);
					}
				}
			}
		}
	}
}

// TODO: free the memory returned by this
unsigned char *CreateEthernetPacket(unsigned char destMAC, unsigned char srcMAC, unsigned char type, unsigned char length, char *payload)
{
	unsigned char *returnPacket = calloc(MAX_ETHERNET_PACKET_SIZE, sizeof(unsigned char));

	returnPacket[0] = destMAC;
	returnPacket[1] = srcMAC;
	returnPacket[2] = type;
	returnPacket[3] = length;

	memcpy((void *)&returnPacket[4], (void *)payload, ETHERNET_PAYLOAD_SIZE);

	return returnPacket;
}

int GetEthernetPacketDestMAC(unsigned char *packet)
{
	int returnValue = packet[0];

	return returnValue;
}

int GetEthernetPacketSourceMAC(unsigned char *packet)
{
	int returnValue = packet[1];

	return returnValue;
}

// TODO: free the memory returned by this
char *GetPayload(char *packet)
{
	char *payload = calloc(ETHERNET_PAYLOAD_SIZE, sizeof(char));

	memcpy(payload, packet + 4, 100);

	return payload;
}

char *GetIPPayload(char *packet)
{
	char *payload = calloc(IP_PAYLOAD_SIZE, sizeof(char));

	memcpy(payload, packet + 7, IP_PAYLOAD_SIZE - 7);

	return payload;
}

char *GetTCPPayload(char *packet)
{
	char *payload = calloc(TCP_PAYLOAD_SIZE, sizeof(char));

	memcpy(payload, packet + 4, TCP_PAYLOAD_SIZE - 4);

	return payload;
}

// TODO: free the memory returned by this
unsigned char *CreateIPPacket(unsigned char length, unsigned char ttl, unsigned char protocol, unsigned char sourceNet, unsigned char sourceHost, unsigned char destNet, unsigned char destHost, unsigned char *payload)
{
	unsigned char *returnPacket = calloc(MAX_IP_PACKET_SIZE, sizeof(unsigned char));

	returnPacket[0] = length;
	returnPacket[1] = ttl;
	returnPacket[2] = protocol;
	returnPacket[3] = sourceNet;
	returnPacket[4] = sourceHost;
	returnPacket[5] = destNet;
	returnPacket[6] = destHost;

	memcpy((void *)&returnPacket[7], (void *)payload, IP_PAYLOAD_SIZE);

	return returnPacket;
}
