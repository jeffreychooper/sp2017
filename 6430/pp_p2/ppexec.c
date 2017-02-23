#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#define HOSTNAME_LENGTH 1024
#define CWD_LENGTH 1024
#define MIN_PORT 4500
#define MAX_PORT 4599
#define DEFAULT_BACKLOG 5
#define SELECT_TIMEOUT 1

typedef struct
{
	int globalRank;
	char *host;
	int port;
} RankInfo;

char *MakeCommandString(int rank, int numRanks, int numHosts, char *mainHostname, int userCommandLength, char *userCommandString);
void ErrorCheck(int val, char *str);
int SetupAcceptSocket();
int AcceptConnection(int acceptSocket);

int main(int argc, char *argv[])
{
	char *hostsfilePath = "hostnames";
	int numRanks = 1;

	// set mainHost to the host on which p1 is run
	char mainHostname[HOSTNAME_LENGTH];
	mainHostname[HOSTNAME_LENGTH - 1] = '\0';		// if the hostname gets truncated, it isn't specified whether it will be null-terminated
	if(!gethostname(mainHostname, HOSTNAME_LENGTH - 1));

	// get command-line arguments
	int currArg = 1;

	char *userCommandString;
	int userCommandLength;
	int userCommandStart;

	while(argc > currArg)
	{
		if(argv[currArg][0] == '-')
		{
			switch(argv[currArg][1])
			{
				case 'f':
					hostsfilePath = strdup(argv[currArg + 1]);
					break;
				case 'n':
					numRanks = atoi(argv[currArg + 1]);
					break;
			}

			currArg += 2;
		}
		else
		{
			// this is the first part of the user's command
			userCommandStart = currArg;
			userCommandLength = strlen(argv[currArg]) + 1;
			currArg++;
			break;
		}
	}
	
	// find the length of the user's command and make a string out of it
	while(argc > currArg)
	{
		userCommandLength += strlen(argv[currArg]) + 1;
		currArg++;
	}

	userCommandString = malloc((userCommandLength) * sizeof(char));
	int userCommandOffset = 0;

	for(int i = userCommandStart; i < argc; i++)
	{
		strncpy(userCommandString + userCommandOffset, argv[i], strlen(argv[i]));
		userCommandOffset += strlen(argv[i]);
		strncpy(userCommandString + userCommandOffset, " ", 1);
		userCommandOffset++;
	}

	// overwrite last space with null-termination
	strncpy(userCommandString + userCommandOffset - 1, "\0", 1);

	// try to open up hosts file
	FILE *hostsfile = fopen(hostsfilePath, "r");
	int numHosts = 0;		// IF I DON'T INITIALIZE THIS IT GIVES ME GARBAGE! >:|
	char **hostnames;

	if(hostsfile != NULL)
	{
		struct stat fileStats;
		off_t fileSize;

		if(stat(hostsfilePath, &fileStats))
		{
			printf("stat failed on %s: %s\n", hostsfile, strerror(errno));
			return 1;
		}
		else
		{
			fileSize = fileStats.st_size;
		}

		// count number of hosts
		char c = fgetc(hostsfile);

		while(c != EOF)
		{
			if(c == '\n')
				numHosts++;

			c = fgetc(hostsfile);
		}

		// fill in the host array
		hostnames = malloc(numHosts * sizeof(char *));

		rewind(hostsfile);

		c = fgetc(hostsfile);
		int hostCount = 0;
		int hostStartPos = 0;
		int hostEndPos;

		while(c != EOF)
		{
			if(c == '\n')
			{
				hostEndPos = ftell(hostsfile) - 2;
				hostnames[hostCount] = malloc((hostEndPos - hostStartPos + 2) * sizeof(char));
				fseek(hostsfile, hostStartPos, SEEK_SET);
				fread((void *)hostnames[hostCount], 1, hostEndPos - hostStartPos + 1, hostsfile);
				hostnames[hostCount][hostEndPos - hostStartPos + 1] = '\0';

				hostStartPos = hostEndPos + 2;
				fseek(hostsfile, hostEndPos + 2, SEEK_SET);

				hostCount++;
			}

			c = fgetc(hostsfile);
		}

		fclose(hostsfile);
	}

	// get the listening port ready
	int acceptSocket = SetupAcceptSocket();

	// make the bash command that will be used to run the user's program
	// ex: bash -c "PP_MPI_RANK={rank} PP_MPI_SIZE={numHosts} PP_MPI_HOST_PORT={mainHost}:9999 {program and args entered by the user}"
	char *command;
	int forkRC;

	// run the program
	if(numHosts == 0)
	{
		// no hosts specified... run locally
		for(int i = 0; i < numRanks; i++)
		{
			command = MakeCommandString(i, numRanks, 1, mainHostname, userCommandLength, userCommandString);

			forkRC = fork();

			if(forkRC == 0)
			{
				char *args[] = {"/bin/bash", "-c", command, NULL};

				execv("/bin/bash", args);
			}

			free(command);
		}
	}
	else
	{
		for(int i = 0; i < numRanks; i++)
		{
			command = MakeCommandString(i, numRanks, numHosts, mainHostname, userCommandLength, userCommandString);

			forkRC = fork();

			if(forkRC == 0)
			{
				char *args[] = {"/usr/bin/ssh", hostnames[i % numHosts], command, NULL};

				execv("/usr/bin/ssh", args);
			}

			free(command);
		}
	}

	free(userCommandString);

	for(int i = 0; i < numHosts; i++)
	{
		free(hostnames[i]);
	}

	// alternate between wait and select... taking care of business
	int rc;
	int pid;
	int processesDone = 0;
	int done = 0;
	RankInfo ranks[numRanks];
	int connectionMatrix[numRanks][numRanks];
	fd_set readFDs;
	struct timeval tv;
	int fdSetSize = 0;

	tv.tv_sec = SELECT_TIMEOUT;

	for(int i = 0; i < numRanks; i++)
		for(int j = 0; j < numRanks; j++)
			connectionMatrix[i][j] = 0;

	while(!done)
	{
		if(pid = waitpid(-1, NULL, WNOHANG))
			processesDone++;

		if(processesDone > numRanks)
			break;

		// select
		FD_ZERO(&readFDs);

		FD_SET(acceptSocket, &readFDs);

		if(acceptSocket > fdSetSize)
			fdSetSize = acceptSocket;

		fdSetSize++;

		rc = select(fdSetSize, &readFDs, NULL, NULL, &tv);

		if(rc == -1 && errno == EINTR)
			continue;

		if(FD_ISSET(acceptSocket, &readFDs))
		{
			// accept connection
			// need to keep track of who it is so we can check for later messages on the socket we open
			int newSocketFD = AcceptConnection(acceptSocket);
		}
	}

	return 0;
}

char *MakeCommandString(int rank, int numRanks, int numHosts, char *mainHostname, int userCommandLength, char *userCommandString)
{
	int stringLength = 0;
	int rankDigits;
	int sizeDigits;

	char *cwd;

	// will be run using ssh across multiple machines
	if(numHosts > 1)
	{
		cwd = getcwd(NULL, 0);

		// bash -c 'PWD=
		stringLength += 13;

		// cwd
		stringLength += strlen(cwd);

		// ; cd_
		stringLength += 5;

		// cwd
		stringLength += strlen(cwd);

		// ;_
		stringLength += 2;

		// the closing '
		stringLength++;
	}

	// PP_MPI_RANK= (12 chars)
	stringLength += 12;

	// assuming we don't get to quadruple digit numbers of ranks
	if(rank > 99)
	{
		stringLength += 3;
		rankDigits = 3;
	}
	else if(rank > 9)
	{
		stringLength += 2;
		rankDigits = 2;
	}
	else
	{
		stringLength++;
		rankDigits = 1;
	}
	
	// _PP_MPI_SIZE= (13 chars)
	stringLength += 13;

	if(numRanks > 99)
	{
		stringLength += 3;
		sizeDigits = 3;
	}
	else if(numRanks > 9)
	{
		stringLength += 2;
		sizeDigits = 2;
	}
	else
	{
		stringLength++;
		sizeDigits = 1;
	}

	// _PP_MPI_HOST_PORT= (18 chars)
	stringLength += 18;

	stringLength += strlen(mainHostname);

	// :9999_ (6 chars)
	stringLength += 6;

	stringLength += userCommandLength - 1;

	// null termitation...
	stringLength++;

	// concatenate it all together...
	char *returnString = malloc(stringLength * sizeof(char));

	int stringPos = 0;

	if(numHosts > 1)
	{
		strncpy(returnString, "bash -c 'PWD=", 13);
		stringPos += 13;

		strncpy(returnString + stringPos, cwd, strlen(cwd));
		stringPos += strlen(cwd);

		strncpy(returnString + stringPos, "; cd ", 5);
		stringPos += 5;

		strncpy(returnString + stringPos, cwd, strlen(cwd));
		stringPos += strlen(cwd);

		strncpy(returnString + stringPos, "; ", 2);
		stringPos += 2;

		free(cwd);
	}

	strncpy(returnString + stringPos, "PP_MPI_RANK=",  12);
	stringPos += 12;
	snprintf(returnString + stringPos, rankDigits + 1, "%d", rank);			// note the +1 here for the null-terminating character...
	stringPos += rankDigits;

	strncpy(returnString + stringPos, " PP_MPI_SIZE=", 13);
	stringPos += 13;
	snprintf(returnString + stringPos, sizeDigits + 1, "%d", numRanks);
	stringPos += sizeDigits;

	strncpy(returnString + stringPos, " PP_MPI_HOST_PORT=", 18);
	stringPos += 18;
	strncpy(returnString + stringPos, mainHostname, strlen(mainHostname));
	stringPos += strlen(mainHostname);
	strncpy(returnString + stringPos, ":9999 ", 6);
	stringPos += 6;

	strncpy(returnString + stringPos, userCommandString, userCommandLength - 1);
	stringPos += userCommandLength - 1;

	if(numHosts > 1)
	{
		strncpy(returnString + stringPos, "'", 1);
		stringPos++;
	}

	strncpy(returnString + stringPos, "\0", 1);

	return returnString;
}

void ErrorCheck(int val, char *str)
{
    if(val < 0)
    {
        printf("%s : %d : %s\n", str, val, strerror(errno));
        exit(1);
    }
}

int SetupAcceptSocket()
{
	int rc;

	int acceptSocket = socket(AF_INET, SOCK_STREAM, 0);
	ErrorCheck(acceptSocket, "SetupAcceptSocket socket");

	int optVal = 1;
	setsockopt(acceptSocket, SOL_SOCKET, SO_REUSEADDR, (char *)&optVal, sizeof(optVal));

	struct sockaddr_in sin;
	sin.sin_family = AF_INET;
	sin.sin_addr.s_addr = INADDR_ANY;

	for(int attemptPort = MIN_PORT; attemptPort <= MAX_PORT; attemptPort++)
	{
		sin.sin_port = htons(attemptPort);

		rc = bind(acceptSocket, (struct sockaddr *)&sin, sizeof(sin));

		if(!rc)
			break;
	}

	rc = listen(acceptSocket, DEFAULT_BACKLOG);
	ErrorCheck(rc, "SetupAcceptSocket listen");

	return acceptSocket;
}

int AcceptConnection(int acceptSocket)
{
	int newSocket;
	struct sockaddr_in from;
	socklen_t fromLen = sizeof(from);

	int success = 0;

	while(!success)
	{
		newSocket = accept(acceptSocket, (struct sockaddr *)&from, &fromLen);

		if(newSocket == -1)
		{
			if(errno == EINTR)
				continue;

			ErrorCheck(newSocket, "AcceptConnection accept");
		}
		else
			success = 1;
	}

	int optVal = 1;
	setsockopt(newSocket, IPPROTO_TCP, TCP_NODELAY, (char *)&optVal, sizeof(optVal));

	return newSocket;
}
