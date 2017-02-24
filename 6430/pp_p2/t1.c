#include <stdio.h>
#include "mpi.h"
#include <math.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
    int i, j, n, myrank, numranks, rc;
    MPI_Status status;
    char hostname[128];

    rc = MPI_Init(NULL,NULL);

	rc = MPI_Finalize();

	return 0;
}
