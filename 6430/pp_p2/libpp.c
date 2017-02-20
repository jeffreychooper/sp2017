#include "mpi.h"

int MPI_Init(int *argc, char ***argv)
{
	return 0;
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
