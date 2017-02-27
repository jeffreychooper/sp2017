// TODO: make changes so my output can print device names
// TODO: sending hosts also need to print a message
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

#define NAME_LENGTH 64
#define MAX_LINE_LENGTH 896
#define CONFIG_MAX_SIZE 42
#define OPERATIONS_START_ELEMENTS 20
#define MAX_LINE_TOKENS 14
#define MAX_TOKEN_LENGTH 64
#define EXTRA_OPERATIONS 10
#define MAX_ETHERNET_PACKET_SIZE 104
#define PAYLOAD_SIZE 100

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

} RouterInfo;

// host information
typedef struct {

char *name;
unsigned char MAC;
unsigned char netIP;
unsigned char hostIP;
int interface;
int interpreterFD;

} HostInfo;

void FreeOperationsMemory(char ***operations, int currMaxOperations);
void ActAsSwitch(SwitchInfo *switchInfo);
void ActAsRouter(RouterInfo *routerInfo);
void ActAsHost(HostInfo *hostInfo);
unsigned char *CreateEthernetPacket(unsigned char destMAC, unsigned char srcMAC, unsigned char type, unsigned char length, char *payload);
int GetEthernetPacketDestMAC(unsigned char *packet);
char *GetPayload(char *packet);

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
						charBuffer[0] = 2;
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
						charBuffer[0] = 2;
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

		operationsIndex++;
	}

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

			if(buffer[0] = 1)
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

		rc = select(setSize, &readFDs, NULL, NULL, NULL);

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
				printf("%s: macsend from %s on %d: %s\n", routerInfo->name, "temp", (int)routerInfo->MACs[interfaceIndex], payload);
				free(payload);
			}

			interfaceIndex++;
		}

		if(!fdWasSet && FD_ISSET(routerInfo->interpreterFD, &readFDs))
		{
			char charBuffer[1];

			read(routerInfo->interpreterFD, (void *)&buffer, 1);

			// (2) for macsend, (1) for done
			if(buffer[0] == 1)
			{
				done = 1;
			}
			else if(buffer[0] == 2)
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
														  4 + strlen(message),
														  message);

				char *payload = GetPayload(packetToSend);
				printf("%s: macsend to %s on %d: %s\n", routerInfo->name, "temp", (int)sender[0], payload);
				free(payload);

				int bytesWritten = write(sendInterface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

				while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
					bytesWritten += write(sendInterface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

				free(packetToSend);
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

		rc = select(setSize, &readFDs, NULL, NULL, NULL);

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
					printf("%s: macsend from %s on %d: %s\n", hostInfo->name, "temp", (int)hostInfo->MAC, payload);
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
			if(buffer[0] == 1)
			{
				done = 1;
			}
			else if(buffer[0] == 2)
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
														  4 + strlen(message),
														  message);

				char *payload = GetPayload(packetToSend);
				printf("%s: macsend to %s on %d: %s\n", hostInfo->name, "temp", (int)sender[0], payload);
				free(payload);

				int bytesWritten = write(hostInfo->interface, (void *)packetToSend, MAX_ETHERNET_PACKET_SIZE);

				while(bytesWritten < MAX_ETHERNET_PACKET_SIZE)
					bytesWritten += write(hostInfo->interface, (void *)packetToSend + bytesWritten, MAX_ETHERNET_PACKET_SIZE - bytesWritten);

				free(packetToSend);
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

	strncpy(&returnPacket[4], payload, strlen(payload));

	return returnPacket;
}

int GetEthernetPacketDestMAC(unsigned char *packet)
{
	int returnValue = packet[0];

	return returnValue;
}

// TODO: free the memory returned by this
char *GetPayload(char *packet)
{
	char *payload = calloc(PAYLOAD_SIZE, sizeof(char));

	strncpy(payload, packet + 4, 100);

	return payload;
}
