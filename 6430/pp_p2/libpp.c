#include "mpi.h"
#include <stdlib.h>
#include <string.h>

static int MPI_World_rank;
static int MPI_World_size;
static char *MPI_Control_host;		// TODO: free in finalize
static int MPI_Control_port;
static char *MPI_My_host;
static int MPI_My_port;

int MPI_Init(int *argc, char ***argv)
{
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

	// get an accept/listen socket and figure out its port

	// connect to ppexec and tell it my host/port

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
