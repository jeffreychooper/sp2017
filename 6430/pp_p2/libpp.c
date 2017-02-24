#include "mpi.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/tcp.h>

#define HOSTNAME_LENGTH 1024
#define DEFAULT_BACKLOG 5

static int MPI_World_rank;
static int MPI_World_size;
static char *MPI_Control_host;		// TODO: free in finalize
static int MPI_Control_port;
static char MPI_My_host[HOSTNAME_LENGTH];
static int MPI_My_listen_port;

static int MPI_Control_socket;
static int MPI_My_accept_socket;

void ErrorCheck(int val, char *str);
int SetupAcceptSocket();

int MPI_Init(int *argc, char ***argv)
{
	int rc;

	// get rank, size, host, port from environment
	MPI_World_rank = atoi(getenv("PP_MPI_RANK"));
	MPI_World_size = atoi(getenv("PP_MPI_SIZE"));

	char *p;
	char *tempString = malloc((strlen(getenv("PP_MPI_HOST_PORT")) + 1) * sizeof(char));
	strncpy(tempString, getenv("PP_MPI_HOST_PORT"), strlen(getenv("PP_MPI_HOST_PORT")) + 1);

	if(p = strtok(tempString, ":"))
	{
		MPI_Control_host = malloc((strlen(p) + 1) * sizeof(char));
		strncpy(MPI_Control_host, p, strlen(p) + 1);

		if(p = strtok(NULL, " "))
		{
			MPI_Control_port = atoi(p);
		}
	}

	// get an accept/listen socket and figure out its host/port
	MPI_My_accept_socket = SetupAcceptSocket();

	rc = gethostname(MPI_My_host, HOSTNAME_LENGTH);

	struct sockaddr_in sin;
	socklen_t sinLength = sizeof(sin);
	rc = getsockname(MPI_My_accept_socket, (struct sockaddr *)&sin, &sinLength);
	ErrorCheck(rc, "MPI_Init getsockname");
	MPI_My_listen_port = sin.sin_port;

	// connect to ppexec
	struct sockaddr_in listener;
	struct hostent *hp;
	int optVal = 1;

	hp = gethostbyname(MPI_Control_host);

	if(!hp)
	{
        printf("%s : %s\n", "MPI_Init gethostbyname", strerror(errno));
        exit(1);
	}

	memset((void *)&listener, '0', sizeof(listener));
	memcpy((void *)&listener.sin_addr, (void *)hp->h_addr, hp->h_length);
	listener.sin_family = hp->h_addrtype;
	listener.sin_port = htons(MPI_Control_port);

	MPI_Control_socket = socket(AF_INET, SOCK_STREAM, 0);
	ErrorCheck(MPI_Control_socket, "MPI_Init socket");

	setsockopt(MPI_Control_socket, IPPROTO_TCP, TCP_NODELAY, (char *)&optVal, sizeof(optVal));

	rc = connect(MPI_Control_socket, (struct sockaddr *)&listener, sizeof(listener));
	ErrorCheck(rc, "MPI_Init connect");

	// tell ppexec my rank, host, and port
	write(MPI_Control_socket, (void *)&MPI_World_rank, sizeof(int));

	int myHostLength = HOSTNAME_LENGTH;
	write(MPI_Control_socket, (void *)&myHostLength, sizeof(int));
	write(MPI_Control_socket, (void *)&MPI_My_host, myHostLength);

	write(MPI_Control_socket, (void *)&MPI_My_listen_port, sizeof(int));

	return MPI_SUCCESS;
}

int MPI_Finalize(void)
{
	return 0;
}

int MPI_Barrier(MPI_Comm comm)
{
	return 0;
}

int MPI_Comm_rank(MPI_Comm comm, int *rank)
{
	return 0;
}

int MPI_Comm_size(MPI_Comm comm, int *size)
{
	return 0;
}

int MPI_Send(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm)
{
	return 0;
}

int MPI_Ssend(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm)
{
	return 0;
}

int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status)
{
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

	sin.sin_port = 0;		// okay with any port

	rc = bind(acceptSocket, (struct sockaddr *)&sin, sizeof(sin));

	ErrorCheck(rc, "SetupAcceptSocket bind");

	rc = listen(acceptSocket, DEFAULT_BACKLOG);
	ErrorCheck(rc, "SetupAcceptSocket listen");

	return acceptSocket;
}
