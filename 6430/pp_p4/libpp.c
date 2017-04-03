// TODO: need to adjust send and receive... make a linked list pointing to buffers where I store things to send and receive... works better with progress engine
// TODO: progress engine might need to be expanded... get info from it about whether who sent us what message
// TODO: I think non-blocking receive is broken for MPI_ANY_SOURCE
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
#include <sys/time.h>

#define HOSTNAME_LENGTH 1024
#define MIN_PORT 4500
#define MAX_PORT 4599
#define DEFAULT_BACKLOG 5

#define CONNECT_FLAG 1
#define QUIT_FLAG 2
#define BARRIER_FLAG 3
#define RECV_READY_FLAG 4
#define SEND_FLAG 5
#define LOOP_ONE_FLAG 6
#define LOOP_TWO_FLAG 7
#define GATHER_ROOT_READY_FLAG 8
#define WRONG_TAG_FLAG 9

#define REQUEST_SEND_TYPE 0
#define REQUEST_RECV_TYPE 1

#define PE_DONT_HANG -1

typedef struct 
{
	MPI_Comm id;
	MPI_Comm parentID;
	int rank;
	int size;
	int *rankSockets;
} MPI_Comm_info;

typedef struct MPI_Request_info MPI_Request_info;

struct MPI_Request_info
{
	int requestID;
	int type;		// 0 send 1 receive
	int finished;
	int targetFD;
	void *buf;
	int count;
	MPI_Datatype datatype;
	int other;
	int tag;
	MPI_Comm comm;
	MPI_Request_info* prevInfoPointer;
	MPI_Request_info* nextInfoPointer;
};

int MPI_World_rank;
int MPI_World_size;
char *MPI_Control_host;
int MPI_Control_port;
char MPI_My_host[HOSTNAME_LENGTH];
int MPI_My_listen_port;

int MPI_Control_socket;
int MPI_My_accept_socket;
int *MPI_Rank_sockets;

int MPI_Num_user_comms;
MPI_Comm_info *MPI_User_comms;

int MPI_Num_requests = 0;
MPI_Request_info *MPI_First_request_pointer;
MPI_Request_info *MPI_Last_request_pointer;

void ErrorCheck(int val, char *str);
int SetupAcceptSocket();
int ProgressEngine(int blockingSocket);		// sometimes, we'll want to block on some certain socket... so we'll select on it in particular. will return 0 when successful
void DoubleLoop(MPI_Comm comm, int rank, int size, int root);
void WriteToCommRank(MPI_Comm comm, int rank, void *buf, size_t count);
void ReadFromCommRank(MPI_Comm comm, int rank, void *buf, size_t count);
int ConnectedToCommRank(MPI_Comm comm, int dest);
void ConnectToCommRank(MPI_Comm comm, int dest);
int GetFDForCommRank(MPI_Comm comm, int dest);
int AddRequestInfo(int type, int targetFD, void *buf, int count, MPI_Datatype datatype, int other, int tag, MPI_Comm comm);		// returns the requestID
void DeleteRequestInfo(MPI_Request_info *request);

int MPI_Init(int *argc, char ***argv)
{
	setbuf(stdout, NULL);

	int rc;

	// set the number of user created communicators to 0
	MPI_Num_user_comms = 0;
	MPI_User_comms = NULL;

	// get rank, size, host, port from environment
	MPI_World_rank = atoi(getenv("PP_MPI_RANK"));
	MPI_World_size = atoi(getenv("PP_MPI_SIZE"));

	// get space for connections to others in comm_world
	MPI_Rank_sockets = malloc(MPI_World_size * sizeof(int));

	for(int i = 0; i < MPI_World_size; i++)
	{
		MPI_Rank_sockets[i] = -1;
	}

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
	MPI_My_listen_port = ntohs(sin.sin_port);

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
	// close sockets (not ppexec)
	close(MPI_My_accept_socket);

	// free memory
	free(MPI_Control_host);

	// TODO: free MPI_Rank_sockets, MPI_User_comms, and rankSockets in user comms

	// tell ppexec I'm ready
	int quitFlag = QUIT_FLAG;
	write(MPI_Control_socket, (void *)&quitFlag, sizeof(int));

	// select on ppexec until it says we can go
	int rc;
	fd_set readFDs;
	int fdSetSize = MPI_Control_socket + 1;

	while(1)
	{
		FD_ZERO(&readFDs);

		FD_SET(MPI_Control_socket, &readFDs);

		rc = select(fdSetSize, &readFDs, NULL, NULL, NULL);

		if(rc == -1 && errno == EINTR)
			continue;

		if(FD_ISSET(MPI_Control_socket, &readFDs))
		{
			int reply;

			read(MPI_Control_socket, (void *)&reply, sizeof(int));

			if(reply == QUIT_FLAG)
			{
				break;
			}
		}
	}

	// close ppexec socket
	close(MPI_Control_socket);

	return MPI_SUCCESS;
}

// TODO: make work for dupped comms
int MPI_Barrier(MPI_Comm comm)
{
	// tell ppexec I want to join a barrier
	int barrierFlag = BARRIER_FLAG;
	write(MPI_Control_socket, (void *)&barrierFlag, sizeof(int));

	// tell ppexec the communicator I'm a part of
	write(MPI_Control_socket, (void *)&comm, sizeof(MPI_Comm));

	// select on ppexec unit it says we can go
	int rc;
	fd_set readFDs;
	int fdSetSize = MPI_Control_socket + 1;

	while(1)
	{
		FD_ZERO(&readFDs);

		FD_SET(MPI_Control_socket, &readFDs);

		rc = select(fdSetSize, &readFDs, NULL, NULL, NULL);

		if(rc == -1 && errno == EINTR)
			continue;

		if(FD_ISSET(MPI_Control_socket, &readFDs))
		{
			int reply;

			read(MPI_Control_socket, (void *)&reply, sizeof(int));

			if(reply == BARRIER_FLAG)
			{
				break;
			}
		}
	}

	return MPI_SUCCESS;
}

int MPI_Comm_rank(MPI_Comm comm, int *rank)
{
	if(comm == MPI_COMM_WORLD)
		*rank = MPI_World_rank;

	return MPI_SUCCESS;
}

int MPI_Comm_size(MPI_Comm comm, int *size)
{
	if(comm == MPI_COMM_WORLD)
		*size = MPI_World_size;

	return MPI_SUCCESS;
}

int MPI_Send(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm)
{
	int rc;
	int commIndex = -1;

	// validate args
	if(dest >= MPI_World_size)
		return MPI_ERR_RANK;

	if(comm != MPI_COMM_WORLD)
	{
		for(int i = 0; i < MPI_Num_user_comms; i++)
		{
			if(MPI_User_comms[i].id == comm)
			{
				commIndex = i;
				break;
			}
		}

		if(commIndex == -1)
			return MPI_ERR_COMM;
	}

	if(tag < 0 && tag != MPI_ANY_TAG)
		return MPI_ERR_TAG;

	// TODO: fix this section if we do anything other than duping comms
	// if no connection to the destination...
	if(MPI_Rank_sockets[dest] == -1)
	{
		// tell ppexec I need to connect to someone
		int connectFlag = CONNECT_FLAG;
		write(MPI_Control_socket, (void *)&connectFlag, sizeof(int));

		// tell ppexec which comm I'm talking about
		int commWorld = MPI_COMM_WORLD;
		write(MPI_Control_socket, (void *)&commWorld, sizeof(int));

		// tell ppexec which rank I want to connect to
		write(MPI_Control_socket, (void *)&dest, sizeof(int));

		// get the host and port back from ppexec
		char destHost[HOSTNAME_LENGTH];
		int destPort;

		read(MPI_Control_socket, (void *)&destHost, HOSTNAME_LENGTH * sizeof(char));
		read(MPI_Control_socket, (void *)&destPort, sizeof(int));

		// connect to dest
		struct sockaddr_in listener;
		struct hostent *hp;
		int optVal = 1;

		hp = gethostbyname((char *)&destHost);

		if(!hp)
		{
			printf("%s : %s\n", "MPI_Send gethostbyname", strerror(errno));
			exit(1);
		}

		memset((void *)&listener, '0', sizeof(listener));
		memcpy((void *)&listener.sin_addr, (void *)hp->h_addr, hp->h_length);
		listener.sin_family = hp->h_addrtype;
		listener.sin_port = htons(destPort);

		MPI_Rank_sockets[dest] = socket(AF_INET, SOCK_STREAM, 0);
		ErrorCheck(MPI_Rank_sockets[dest], "MPI_Send socket");

		setsockopt(MPI_Rank_sockets[dest], IPPROTO_TCP, TCP_NODELAY, (char *)&optVal, sizeof(optVal));

		rc = connect(MPI_Rank_sockets[dest], (struct sockaddr *)&listener, sizeof(listener));
		ErrorCheck(rc, "MPI_Send connect");

		// tell the dest which global rank I am
		write(MPI_Rank_sockets[dest], (void *)&MPI_World_rank, sizeof(int));

		// TODO: this will also need to change
		// propagate changes to other comms
		for(int i = 0; i < MPI_Num_user_comms; i++)
		{
			if(MPI_User_comms[i].parentID == MPI_COMM_WORLD)
			{
				MPI_User_comms[i].rankSockets[dest] = MPI_Rank_sockets[dest];
			}
		}
	}

	// tell dest we're sending
	int sendFlag = SEND_FLAG;
	WriteToCommRank(comm, dest, (void *)&sendFlag, sizeof(int));

	// tell dest the comm we're using
	WriteToCommRank(comm, dest, (void *)&comm, sizeof(MPI_Comm));

	// tell dest our rank
	if(comm == MPI_COMM_WORLD)
		WriteToCommRank(comm, dest, (void *)&MPI_World_rank, sizeof(int));
	else
		WriteToCommRank(comm, dest, (void *)&MPI_User_comms[commIndex].rank, sizeof(int));

	// tell dest the tag we're using
	WriteToCommRank(comm, dest, (void *)&tag, sizeof(int));

	// returns 0 when it's ready
	if(comm == MPI_COMM_WORLD)
	{
		while(ProgressEngine(MPI_Rank_sockets[dest]))
			;
	}
	else
	{
		while(ProgressEngine(MPI_User_comms[commIndex].rankSockets[dest]))
			;
	}

	int typeSize;

	if(datatype == MPI_CHAR)
		typeSize = sizeof(char);
	else if(datatype == MPI_INT)
		typeSize = sizeof(int);

	int bytesToWrite = count * typeSize;

	WriteToCommRank(comm, dest, buf, bytesToWrite);

	return MPI_SUCCESS;
}

int MPI_Ssend(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm)
{
	MPI_Send(buf, count, datatype, dest, tag, comm);

	return MPI_SUCCESS;
}

int MPI_Recv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status)
{
	status->count = count;

	if(source != MPI_ANY_SOURCE)
	{
		// TODO: will need to fix for split comms...
		// if we don't have a connection yet, do the progress engine (which will do the accept socket work)
		while(MPI_Rank_sockets[source] == -1)
		{
			// returns 0 when it gets something from the specified socket
			while(ProgressEngine(MPI_My_accept_socket))
				;
		}

		// TODO: just throws away messages on a comm it isn't expecting
		// read from the sender until it tells me the tag it's sending
		int senderComm;

		while(senderComm != comm);
		{
			int senderFlag;
			while(senderFlag != SEND_FLAG)
				ReadFromCommRank(comm, source, (void *)&senderFlag, sizeof(int));

			ReadFromCommRank(comm, source, (void *)&senderComm, sizeof(int));
		}

		int senderSource;
		ReadFromCommRank(comm, source, (void *)&senderSource, sizeof(int));

		int senderTag;
		ReadFromCommRank(comm, source, (void *)&senderTag, sizeof(int));

		status->MPI_SOURCE = senderSource;
		status->MPI_TAG = senderTag;

		if(tag == MPI_ANY_TAG || senderTag == tag)
		{
			int readyFlag = RECV_READY_FLAG;
			WriteToCommRank(comm, source, (void *)&readyFlag, sizeof(int));

			int typeSize;

			if(datatype == MPI_CHAR)
				typeSize = sizeof(char);
			else if(datatype == MPI_INT)
				typeSize = sizeof(int);

			int bytesToRead = count * typeSize;

			ReadFromCommRank(comm, source, buf, bytesToRead);
		}
		else
		{
			return MPI_ERR_TAG;
		}
	}
	else
	{
		int rc;
		fd_set readFDs;
		int fdSetSize = 0;

		while(1)
		{
			FD_ZERO(&readFDs);

			FD_SET(MPI_My_accept_socket, &readFDs);

			if(MPI_My_accept_socket > fdSetSize)
				fdSetSize = MPI_My_accept_socket;

			for(int i = 0; i < MPI_World_size; i++)
			{
				int rankSocket = MPI_Rank_sockets[i];
				if(rankSocket != -1)
					FD_SET(rankSocket, &readFDs);

				if(rankSocket > fdSetSize)
					fdSetSize = rankSocket;
			}

			fdSetSize++;

			rc = select(fdSetSize, &readFDs, NULL, NULL, NULL);

			if(rc == -1 && errno == EINTR)
				continue;

			if(FD_ISSET(MPI_My_accept_socket, &readFDs))
			{
				int newSocket;
				struct sockaddr_in from;
				socklen_t fromLen = sizeof(from);

				int success = 0;

				while(!success)
				{
					newSocket = accept(MPI_My_accept_socket, (struct sockaddr *)&from, &fromLen);

					if(newSocket == -1)
					{
						if(errno == EINTR)
							continue;

						ErrorCheck(newSocket, "ProgressEngine accept");
					}
					else
						success = 1;
				}

				int optVal = 1;
				setsockopt(newSocket, IPPROTO_TCP, TCP_NODELAY, (char *)&optVal, sizeof(optVal));

				int newSocketRank;
				read(newSocket, (void *)&newSocketRank, sizeof(int));

				MPI_Rank_sockets[newSocketRank] = newSocket;
			}

			for(int i = 0; i < MPI_World_size; i++)
			{
				int rankSocket = MPI_Rank_sockets[i];

				if(rankSocket != -1 && FD_ISSET(rankSocket, &readFDs))
				{
					int requestFlag;
					read(rankSocket, (void *)&requestFlag, sizeof(int));

					if(requestFlag == SEND_FLAG)
					{
						int senderComm;
						read(rankSocket, (void *)&senderComm, sizeof(int));
						
						if(senderComm != comm)
							continue;

						int senderSource;
						read(rankSocket, (void *)&senderSource, sizeof(int));

						int senderTag;
						read(rankSocket, (void *)&senderTag, sizeof(int));

						status->MPI_SOURCE = senderSource;
						status->MPI_TAG = senderTag;

						if(tag == MPI_ANY_TAG || senderTag == tag)
						{
							int readyFlag = RECV_READY_FLAG;
							write(rankSocket, (void *)&readyFlag, sizeof(int));

							int typeSize;

							if(datatype == MPI_CHAR)
								typeSize = sizeof(char);
							else if(datatype == MPI_INT)
								typeSize = sizeof(int);

							int bytesToRead = count * typeSize;

							int bytesRead = read(rankSocket, buf, bytesToRead);

							while(bytesRead < bytesToRead)
								bytesRead += read(rankSocket, buf + bytesRead, bytesToRead - bytesRead);

							return MPI_SUCCESS;
						}
						else
						{
							return MPI_ERR_TAG;
						}
					}
				}
			}
		}
	}

	return MPI_SUCCESS;
}

double MPI_Wtime(void)
{
	struct timeval tv;

	gettimeofday(&tv, NULL);
	return (tv.tv_sec + (tv.tv_usec / 1000000.0));
}

int MPI_Comm_dup(MPI_Comm comm, MPI_Comm *newComm)
{
	// get reference to old comm
	int oldCommIndex = -1;

	if(comm != MPI_COMM_WORLD)
	{
		for(int i = 0; i < MPI_Num_user_comms; i++)
		{
			if(MPI_User_comms[i].id == comm)
			oldCommIndex = i;
		}

		if(oldCommIndex == -1)
		{
			return MPI_ERR_COMM;
		}
	}

	// create space for the new communicator in MPI_User_comms
	MPI_Num_user_comms++;
	MPI_User_comms = realloc(MPI_User_comms, (MPI_Num_user_comms * sizeof(MPI_Comm_info)));

	int newCommIndex = MPI_Num_user_comms - 1;

	if(comm == MPI_COMM_WORLD)
		MPI_User_comms[newCommIndex].rankSockets = malloc((MPI_World_size * sizeof(int)));
	else
		MPI_User_comms[newCommIndex].rankSockets = malloc((MPI_User_comms[oldCommIndex].size * sizeof(int)));

	// store the info about the new communicator
	if(MPI_Num_user_comms == 1)
	{
		MPI_User_comms[newCommIndex].id = MPI_COMM_WORLD + 1;
		MPI_User_comms[newCommIndex].parentID = MPI_COMM_WORLD;
	}
	else
	{
		MPI_User_comms[newCommIndex].id = MPI_User_comms[newCommIndex - 1].id + 1;
		MPI_User_comms[newCommIndex].parentID = MPI_User_comms[oldCommIndex].id;
	}
		
	if(comm == MPI_COMM_WORLD)
	{
		MPI_User_comms[newCommIndex].rank = MPI_World_rank;
		MPI_User_comms[newCommIndex].size = MPI_World_size;

		for(int i = 0; i < MPI_World_size; i++)
		{
			MPI_User_comms[newCommIndex].rankSockets[i] = MPI_Rank_sockets[i];
		}
	}
	else
	{
		MPI_User_comms[newCommIndex].rank = MPI_User_comms[oldCommIndex].rank;
		MPI_User_comms[newCommIndex].size = MPI_User_comms[oldCommIndex].size;

		for(int i = 0; i < MPI_World_size; i++)
		{
			MPI_User_comms[newCommIndex].rankSockets[i] = MPI_User_comms[oldCommIndex].rankSockets[i];
		}
	}

	// set the newComm given by user
	*newComm = MPI_User_comms[newCommIndex].id;

	// block to make sure everyone is done
	DoubleLoop(*newComm, MPI_User_comms[newCommIndex].rank, MPI_User_comms[newCommIndex].size, 0);

	return MPI_SUCCESS;
}

// TODO: I'm doing this the lazy way... root will just select on the others and put their info in the buffer as it comes
int MPI_Gather(void *sendbuf, int sendcnt, MPI_Datatype sendtype, void *recvbuf, int recvcnt, MPI_Datatype recvtype, int root, MPI_Comm comm)
{
	#if DEBUG
	int debug = 0;
	while(!debug)
		;
	#endif

	if(MPI_World_rank == root)
	{
		// TODO: really bad way of doing this...
		int typeSize;
		if(recvtype == MPI_CHAR)
			typeSize = sizeof(char);
		else if(recvtype == MPI_INT)
			typeSize = sizeof(int);

		int bytesToRead = recvcnt * typeSize;

		for(int i = 0; i < MPI_World_size; i++)
		{
			if(i != root)
			{
				while(!ConnectedToCommRank(comm, i))
				{
					while(ProgressEngine(MPI_My_accept_socket))
						;
				}

				int flag = GATHER_ROOT_READY_FLAG;

				WriteToCommRank(comm, i, (void *)&flag, sizeof(int));
				ReadFromCommRank(comm, i, recvbuf + (i * bytesToRead), bytesToRead); 

				printf("root %d got %d from %d\n", MPI_World_rank, ((int *)recvbuf)[i], i);
			}
		}

		// move the root's own info over
		memcpy(recvbuf + (root * bytesToRead), sendbuf, bytesToRead);
	}
	else
	{
		while(!ConnectedToCommRank(comm, root))
			ConnectToCommRank(comm, root);

		int typeSize;
		if(sendtype == MPI_CHAR)
			typeSize = sizeof(char);
		else if(sendtype == MPI_INT)
			typeSize = sizeof(int);

		int bytesToWrite = sendcnt * typeSize;

		int flag;

		while(flag != GATHER_ROOT_READY_FLAG)
			ReadFromCommRank(comm, root, (void *)&flag, sizeof(int));

		WriteToCommRank(comm, root, sendbuf, bytesToWrite);

		printf("rank %d sent to root %d\n", MPI_World_rank, root);
	}

	DoubleLoop(MPI_COMM_WORLD, MPI_World_rank, MPI_World_size, root);

	return MPI_SUCCESS;
}

// TODO: lazy way....
int MPI_Bcast(void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm)
{
	int typeSize;
	if(datatype == MPI_CHAR)
		typeSize = sizeof(char);
	else if(datatype == MPI_INT)
		typeSize = sizeof(int);

	int bytesToCommunicate = count * typeSize;

	if(MPI_World_rank == root)
	{
		for(int i = 0; i < MPI_World_size; i++)
		{
			if(i != root)
			{
				// make sure we're connected
				if(!ConnectedToCommRank(comm, i))
					ConnectToCommRank(comm, i);

				// send the info over
				WriteToCommRank(comm, i, buffer, bytesToCommunicate);
			}
		}
	}
	else
	{
		// make sure we're connected to root
		while(!ConnectedToCommRank(comm, root))
		{
			while(ProgressEngine(MPI_My_accept_socket))
				;
		}

		// wait for root to give us what we're waiting for
		ReadFromCommRank(comm, root, buffer, bytesToCommunicate);
	}

	DoubleLoop(comm, MPI_World_rank, MPI_World_size, root);

	return MPI_SUCCESS;
}

int MPI_Isend(void *buf, int count, MPI_Datatype datatype, int dest, int tag, MPI_Comm comm, MPI_Request *request)
{
	// if not connected
	if(!ConnectedToCommRank(comm, dest))
	{
		// connect
		ConnectToCommRank(comm, dest);
	}

	// make a note of the fact that the user wants to send something
	int requestID = AddRequestInfo(REQUEST_SEND_TYPE, GetFDForCommRank(comm, dest), buf, count, datatype, dest, tag, comm);
	*request = requestID;

	return MPI_SUCCESS;
}

int MPI_Irecv(void *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Request *request)
{
	if(source != MPI_ANY_SOURCE)
	{
		// while not connected
		while(!ConnectedToCommRank(comm, source))
		{
			// hang on the PE until it's connected
			while(ProgressEngine(MPI_My_accept_socket))
				;
		}
	}

	// make a note of the fact that the user wants to receive something
	int requestID = AddRequestInfo(REQUEST_RECV_TYPE, GetFDForCommRank(comm, source), buf, count, datatype, source, tag, comm);
	*request = requestID;
	
	return MPI_SUCCESS;
}

// TODO: currently this just hangs like wait... but sets the flag for success
int MPI_Test(MPI_Request *request, int *flag, MPI_Status *status)
{
	MPI_Request_info *requestInfo = MPI_First_request_pointer;
	
	while(requestInfo)
	{
		if(requestInfo->requestID == *request)
			break;

		requestInfo = requestInfo->nextInfoPointer;
	}

	if(requestInfo->finished)
	{
		DeleteRequestInfo(requestInfo);
		return MPI_SUCCESS;
	}

	if(requestInfo->type == REQUEST_SEND_TYPE)
	{
		// tell dest we're sending
		int sendFlag = SEND_FLAG;
		WriteToCommRank(requestInfo->comm, requestInfo->other, (void *)&sendFlag, sizeof(int));

		// tell dest the comm we're using
		WriteToCommRank(requestInfo->comm, requestInfo->other, (void *)&(requestInfo->comm), sizeof(MPI_Comm));

		// tell dest our rank
		WriteToCommRank(requestInfo->comm, requestInfo->other, (void *)&MPI_World_rank, sizeof(int));

		// tell dest the tag we're using
		WriteToCommRank(requestInfo->comm, requestInfo->other, (void *)&(requestInfo->tag), sizeof(int));

		while(ProgressEngine(requestInfo->targetFD))
			;

		int typeSize;

		if(requestInfo->datatype == MPI_CHAR)
			typeSize = sizeof(char);
		else if(requestInfo->datatype == MPI_INT)
			typeSize = sizeof(int);

		int bytesToWrite = requestInfo->count * typeSize;

		WriteToCommRank(requestInfo->comm, requestInfo->other, requestInfo->buf, bytesToWrite);
	}
	else if(requestInfo->type == REQUEST_RECV_TYPE)
	{
		while(ProgressEngine(requestInfo->targetFD))
			;

		// TODO: just throws away messages on a comm it isn't expecting
		int senderComm;

		ReadFromCommRank(requestInfo->comm, requestInfo->other, (void *)&senderComm, sizeof(int));

		int senderSource;
		ReadFromCommRank(requestInfo->comm, requestInfo->other, (void *)&senderSource, sizeof(int));

		int senderTag;
		ReadFromCommRank(requestInfo->comm, requestInfo->other, (void *)&senderTag, sizeof(int));

		status->MPI_SOURCE = senderSource;
		status->MPI_TAG = senderTag;

		if(senderComm != requestInfo->comm)
		{
			// TODO: wrong comm... work this out
			return MPI_SUCCESS;
		}

		if(requestInfo->tag != MPI_ANY_TAG && senderTag != requestInfo->tag)
		{
			int wrongTagFlag = WRONG_TAG_FLAG;
			WriteToCommRank(requestInfo->comm, requestInfo->other, (void *)&wrongTagFlag, sizeof(int));

			int desiredTag = requestInfo->tag;
			WriteToCommRank(requestInfo->comm, requestInfo->other, (void *)&desiredTag, sizeof(int));

			ReadFromCommRank(requestInfo->comm, requestInfo->other, (void *)&senderTag, sizeof(int));
		}

		if(requestInfo->tag == MPI_ANY_TAG || senderTag == requestInfo->tag)
		{
			int readyFlag = RECV_READY_FLAG;
			WriteToCommRank(requestInfo->comm, requestInfo->other, (void *)&readyFlag, sizeof(int));

			int typeSize;

			if(requestInfo->datatype == MPI_CHAR)
				typeSize = sizeof(char);
			else if(requestInfo->datatype == MPI_INT)
				typeSize = sizeof(int);

			int bytesToRead = requestInfo->count * typeSize;

			ReadFromCommRank(requestInfo->comm, requestInfo->other, requestInfo->buf, bytesToRead);
		}
	}

	*flag = 1;

	DeleteRequestInfo(requestInfo);

	return MPI_SUCCESS;
}

int MPI_Wait(MPI_Request *request, MPI_Status *status)
{
	MPI_Request_info *requestInfo = MPI_First_request_pointer;
	
	while(requestInfo)
	{
		if(requestInfo->requestID == *request)
			break;

		requestInfo = requestInfo->nextInfoPointer;
	}

	if(requestInfo->finished)
	{
		DeleteRequestInfo(requestInfo);
		return MPI_SUCCESS;
	}

	if(requestInfo->type == REQUEST_SEND_TYPE)
	{
		// tell dest we're sending
		int sendFlag = SEND_FLAG;
		WriteToCommRank(requestInfo->comm, requestInfo->other, (void *)&sendFlag, sizeof(int));

		// tell dest the comm we're using
		WriteToCommRank(requestInfo->comm, requestInfo->other, (void *)&(requestInfo->comm), sizeof(MPI_Comm));

		// tell dest our rank
		WriteToCommRank(requestInfo->comm, requestInfo->other, (void *)&MPI_World_rank, sizeof(int));

		// tell dest the tag we're using
		WriteToCommRank(requestInfo->comm, requestInfo->other, (void *)&(requestInfo->tag), sizeof(int));

		while(ProgressEngine(requestInfo->targetFD))
			;

		int typeSize;

		if(requestInfo->datatype == MPI_CHAR)
			typeSize = sizeof(char);
		else if(requestInfo->datatype == MPI_INT)
			typeSize = sizeof(int);

		int bytesToWrite = requestInfo->count * typeSize;

		WriteToCommRank(requestInfo->comm, requestInfo->other, requestInfo->buf, bytesToWrite);
	}
	else if(requestInfo->type == REQUEST_RECV_TYPE)
	{
		while(ProgressEngine(requestInfo->targetFD))
			;

		// TODO: just throws away messages on a comm it isn't expecting
		int senderComm;

		ReadFromCommRank(requestInfo->comm, requestInfo->other, (void *)&senderComm, sizeof(int));

		int senderSource;
		ReadFromCommRank(requestInfo->comm, requestInfo->other, (void *)&senderSource, sizeof(int));

		int senderTag;
		ReadFromCommRank(requestInfo->comm, requestInfo->other, (void *)&senderTag, sizeof(int));

		status->MPI_SOURCE = senderSource;
		status->MPI_TAG = senderTag;

		if(senderComm != requestInfo->comm)
		{
			// TODO: wrong comm... work this out
			return MPI_SUCCESS;
		}

		if(requestInfo->tag != MPI_ANY_TAG && senderTag != requestInfo->tag)
		{
			int wrongTagFlag = WRONG_TAG_FLAG;
			WriteToCommRank(requestInfo->comm, requestInfo->other, (void *)&wrongTagFlag, sizeof(int));

			int desiredTag = requestInfo->tag;
			WriteToCommRank(requestInfo->comm, requestInfo->other, (void *)&desiredTag, sizeof(int));

			ReadFromCommRank(requestInfo->comm, requestInfo->other, (void *)&senderTag, sizeof(int));
		}

		if(requestInfo->tag == MPI_ANY_TAG || senderTag == requestInfo->tag)
		{
			int readyFlag = RECV_READY_FLAG;
			WriteToCommRank(requestInfo->comm, requestInfo->other, (void *)&readyFlag, sizeof(int));

			int typeSize;

			if(requestInfo->datatype == MPI_CHAR)
				typeSize = sizeof(char);
			else if(requestInfo->datatype == MPI_INT)
				typeSize = sizeof(int);

			int bytesToRead = requestInfo->count * typeSize;

			ReadFromCommRank(requestInfo->comm, requestInfo->other, requestInfo->buf, bytesToRead);
		}
	}

	DeleteRequestInfo(requestInfo);

	return MPI_SUCCESS;
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

	sin.sin_port = 0;

	rc = bind(acceptSocket, (struct sockaddr *)&sin, sizeof(sin));
	ErrorCheck(rc, "SetupAcceptSocket bind");

	rc = listen(acceptSocket, DEFAULT_BACKLOG);
	ErrorCheck(rc, "SetupAcceptSocket listen");

	return acceptSocket;
}

int ProgressEngine(int blockingSocket)
{
	// select on the accept socket (one day, this will also probably select on some array of nonblocking receiving sockets)
	int rc;
	fd_set readFDs;
	int fdSetSize = 0;

	while(1)
	{
		FD_ZERO(&readFDs);

		FD_SET(MPI_My_accept_socket, &readFDs);

		if(MPI_My_accept_socket > fdSetSize)
			fdSetSize = MPI_My_accept_socket;

		for(int i = 0; i < MPI_World_size; i++)
		{
			int rankSocket = MPI_Rank_sockets[i];
			if(rankSocket != -1)
				FD_SET(rankSocket, &readFDs);

			if(rankSocket > fdSetSize)
				fdSetSize = rankSocket;
		}

		fdSetSize++;

		// waiting on some socket in particular
		if(blockingSocket != PE_DONT_HANG)
		{
			rc = select(fdSetSize, &readFDs, NULL, NULL, NULL);

			if(rc == -1 && errno == EINTR)
				continue;

			if(FD_ISSET(MPI_My_accept_socket, &readFDs))
			{
				// handle accepting the new connection
				int newSocket;
				struct sockaddr_in from;
				socklen_t fromLen = sizeof(from);

				int success = 0;

				while(!success)
				{
					newSocket = accept(MPI_My_accept_socket, (struct sockaddr *)&from, &fromLen);

					if(newSocket == -1)
					{
						if(errno == EINTR)
							continue;

						ErrorCheck(newSocket, "ProgressEngine accept");
					}
					else
						success = 1;
				}

				int optVal = 1;
				setsockopt(newSocket, IPPROTO_TCP, TCP_NODELAY, (char *)&optVal, sizeof(optVal));

				// get the global rank of the newly connected friend
				int newSocketRank;
				read(newSocket, (void *)&newSocketRank, sizeof(int));

				MPI_Rank_sockets[newSocketRank] = newSocket;

				if(MPI_My_accept_socket == blockingSocket)
					return 0;
			}

			for(int i = 0; i < MPI_World_size; i++)
			{
				int rankSocket = MPI_Rank_sockets[i];

				if(rankSocket != -1 && FD_ISSET(rankSocket, &readFDs))
				{
					int requestFlag;
					read(rankSocket, (void *)&requestFlag, sizeof(int));

					if(requestFlag == WRONG_TAG_FLAG)
					{
						// read the tag the other rank wants
						int requestedTag;
						read(rankSocket, (void *)&requestedTag, sizeof(int));

						// find the right tag
						MPI_Request_info *requestInfo = MPI_First_request_pointer;

						while(requestInfo)
						{
							if(requestInfo->tag == requestedTag)
								break;

							requestInfo = requestInfo->nextInfoPointer;
						}

						// send the tag again...
						WriteToCommRank(requestInfo->comm, requestInfo->other, (void *)&(requestInfo->tag), sizeof(int));

						// read the ready thing
						read(rankSocket, (void *)&requestFlag, sizeof(int));

						// send the requested stuff
						int typeSize;

						if(requestInfo->datatype == MPI_CHAR)
							typeSize = sizeof(char);
						else if(requestInfo->datatype == MPI_INT)
							typeSize = sizeof(int);

						int bytesToWrite = requestInfo->count * typeSize;

						WriteToCommRank(requestInfo->comm, requestInfo->other, requestInfo->buf, bytesToWrite);

						// set the completed request as finished... continue trying for this one
						requestInfo->finished = 1;

						// send all that info again...
						int sendFlag = SEND_FLAG;
						WriteToCommRank(requestInfo->comm, requestInfo->other, (void *)&sendFlag, sizeof(int));

						// tell dest the comm we're using
						WriteToCommRank(requestInfo->comm, requestInfo->other, (void *)&(requestInfo->comm), sizeof(MPI_Comm));

						// tell dest our rank
						WriteToCommRank(requestInfo->comm, requestInfo->other, (void *)&MPI_World_rank, sizeof(int));

						// tell dest the tag we're using
						WriteToCommRank(requestInfo->comm, requestInfo->other, (void *)&(requestInfo->tag), sizeof(int));

						continue;
					}

					if(requestFlag == RECV_READY_FLAG)
					{
						MPI_Request_info *currentRequest = MPI_First_request_pointer;

						while(currentRequest)
						{
							if(currentRequest->targetFD == MPI_Rank_sockets[i])
							{
								return 0;
							}

							currentRequest = currentRequest->nextInfoPointer;
						}

						return 0;
					}
					else if(requestFlag == SEND_FLAG)
					{
						MPI_Request_info *currentRequest = MPI_First_request_pointer;

						while(currentRequest)
						{
							if(currentRequest->targetFD == MPI_Rank_sockets[i])
							{
								return 0;
							}

							currentRequest = currentRequest->nextInfoPointer;
						}

						return 0;
					}
				}
			}
		}
		else
		{
			// TODO: here where we aren't waiting for a specific socket, I could handle Isends/Irecvs that are ready...
			struct timeval tv;
			tv.tv_sec = 0;

			rc = select(fdSetSize, &readFDs, NULL, NULL, &tv);

			if(rc == -1 && errno == EINTR)
				continue;

			if(FD_ISSET(MPI_My_accept_socket, &readFDs))
			{
				int newSocket;
				struct sockaddr_in from;
				socklen_t fromLen = sizeof(from);

				int success = 0;

				while(!success)
				{
					newSocket = accept(MPI_My_accept_socket, (struct sockaddr *)&from, &fromLen);

					if(newSocket == -1)
					{
						if(errno == EINTR)
							continue;

						ErrorCheck(newSocket, "ProgressEngine accept");
					}
					else
						success = 1;
				}

				int optVal = 1;
				setsockopt(newSocket, IPPROTO_TCP, TCP_NODELAY, (char *)&optVal, sizeof(optVal));

				int newSocketRank;
				read(newSocket, (void *)&newSocketRank, sizeof(int));

				MPI_Rank_sockets[newSocketRank] = newSocket;

				break;
			}
		
			break;
		}
	}

	// if we got this far, assume there was no specific task we're working on
	return -1;
}

// TODO: this is a stupid way of doing this... if the people we're receiving from aren't also in DoubleLoop, it's all wrong...
void DoubleLoop(MPI_Comm comm, int rank, int size, int root)
{
	int toSend;
	int received = 0;
	int prevRank = rank - 1;
	int nextRank = rank + 1;

	if(prevRank == -1)
		prevRank = size - 1;

	if(nextRank == size)
		nextRank = 0;

	if(rank == root)
	{
		// check if we're connected to nextRank
		if(!ConnectedToCommRank(comm, nextRank))
		{
			ConnectToCommRank(comm, nextRank);
		}

		// check if we're connected to prevRank
		while(!ConnectedToCommRank(comm, prevRank))
		{
			while(ProgressEngine(MPI_My_accept_socket))
				;
		}

		toSend = LOOP_ONE_FLAG;
	
		WriteToCommRank(comm, nextRank, (void *)&toSend, sizeof(int));
		while(received != LOOP_ONE_FLAG)
			ReadFromCommRank(comm, prevRank, (void *)&received, sizeof(int));

		toSend = LOOP_TWO_FLAG;

		WriteToCommRank(comm, nextRank, (void *)&toSend, sizeof(int));

		while(received != LOOP_TWO_FLAG)
			ReadFromCommRank(comm, prevRank, (void *)&received, sizeof(int));
	}
	else
	{
		// check if we're connected to prevRank
		while(!ConnectedToCommRank(comm, prevRank))
		{
			while(ProgressEngine(MPI_My_accept_socket))
				;
		}

		// check if we're connected to nextRank
		if(!ConnectedToCommRank(comm, nextRank))
		{
			ConnectToCommRank(comm, nextRank);
		}

		toSend = LOOP_ONE_FLAG;
		while(received != LOOP_ONE_FLAG)
			ReadFromCommRank(comm, prevRank, (void *)&received, sizeof(int));

		WriteToCommRank(comm, nextRank, (void *)&toSend, sizeof(int));

		toSend = LOOP_TWO_FLAG;
		while(received != LOOP_TWO_FLAG)
			ReadFromCommRank(comm, prevRank, (void *)&received, sizeof(int));

		WriteToCommRank(comm, nextRank, (void *)&toSend, sizeof(int));
	}
}

void WriteToCommRank(MPI_Comm comm, int rank, void *buf, size_t count)
{
	int interface;

	if(comm == MPI_COMM_WORLD)
	{
		interface = MPI_Rank_sockets[rank];
	}
	else
	{
		for(int i = 0; i < MPI_Num_user_comms; i++)
		{
			if(MPI_User_comms[i].id == comm)
			{
				// TODO: fix this... like readfromcommrank
				interface = MPI_Rank_sockets[rank];
				break;
			}
		}
	}

	int bytesWritten = write(interface, buf, count);

	while(bytesWritten < count)
		bytesWritten += write(interface, buf + bytesWritten, count - bytesWritten);
}

void ReadFromCommRank(MPI_Comm comm, int rank, void *buf, size_t count)
{
	int interface;

	if(comm == MPI_COMM_WORLD)
	{
		interface = MPI_Rank_sockets[rank];
	}
	else
	{
		for(int i = 0; i < MPI_Num_user_comms; i++)
		{
			if(MPI_User_comms[i].id == comm)
			{
				// TODO: fix this... instead of just using world
				interface = MPI_Rank_sockets[rank];
				break;
			}
		}
	}

	int bytesRead = read(interface, buf, count);

	while(bytesRead < count)
		bytesRead += read(interface, buf + bytesRead, count - bytesRead);
}

int ConnectedToCommRank(MPI_Comm comm, int dest)
{
	return MPI_Rank_sockets[dest] != -1;
}

void ConnectToCommRank(MPI_Comm comm, int dest)
{
	int rc;

	// TODO: fix this section if we do anything other than duping comms
	// tell ppexec I need to connect to someone
	int connectFlag = CONNECT_FLAG;
	write(MPI_Control_socket, (void *)&connectFlag, sizeof(int));

	// tell ppexec which comm I'm talking about
	int commWorld = MPI_COMM_WORLD;
	write(MPI_Control_socket, (void *)&commWorld, sizeof(int));

	// tell ppexec which rank I want to connect to
	write(MPI_Control_socket, (void *)&dest, sizeof(int));

	// get the host and port back from ppexec
	char destHost[HOSTNAME_LENGTH];
	int destPort;

	read(MPI_Control_socket, (void *)&destHost, HOSTNAME_LENGTH * sizeof(char));
	read(MPI_Control_socket, (void *)&destPort, sizeof(int));

	// connect to dest
	struct sockaddr_in listener;
	struct hostent *hp;
	int optVal = 1;

	hp = gethostbyname((char *)&destHost);

	if(!hp)
	{
		printf("%s : %s\n", "MPI_Send gethostbyname", strerror(errno));
		exit(1);
	}

	memset((void *)&listener, '0', sizeof(listener));
	memcpy((void *)&listener.sin_addr, (void *)hp->h_addr, hp->h_length);
	listener.sin_family = hp->h_addrtype;
	listener.sin_port = htons(destPort);

	MPI_Rank_sockets[dest] = socket(AF_INET, SOCK_STREAM, 0);
	ErrorCheck(MPI_Rank_sockets[dest], "MPI_Send socket");

	setsockopt(MPI_Rank_sockets[dest], IPPROTO_TCP, TCP_NODELAY, (char *)&optVal, sizeof(optVal));

	rc = connect(MPI_Rank_sockets[dest], (struct sockaddr *)&listener, sizeof(listener));
	ErrorCheck(rc, "MPI_Send connect");

	// tell the dest which global rank I am
	write(MPI_Rank_sockets[dest], (void *)&MPI_World_rank, sizeof(int));

	// TODO: this will also need to change
	// propagate changes to other comms
	for(int i = 0; i < MPI_Num_user_comms; i++)
	{
		if(MPI_User_comms[i].parentID == MPI_COMM_WORLD)
		{
			MPI_User_comms[i].rankSockets[dest] = MPI_Rank_sockets[dest];
		}
	}
}

int GetFDForCommRank(MPI_Comm comm, int dest)
{
	int interface;

	if(comm == MPI_COMM_WORLD)
	{
		interface = MPI_Rank_sockets[dest];
	}
	else
	{
		for(int i = 0; i < MPI_Num_user_comms; i++)
		{
			if(MPI_User_comms[i].id == comm)
			{
				// TODO: fix this... instead of just using world
				interface = MPI_Rank_sockets[dest];
				break;
			}
		}
	}

	return interface;
}

int AddRequestInfo(int type, int targetFD, void *buf, int count, MPI_Datatype datatype, int other, int tag, MPI_Comm comm)
{
	MPI_Request_info *newRequest = malloc(sizeof(MPI_Request_info));

	if(MPI_Num_requests == 0)
	{
		newRequest->requestID = 0;
		newRequest->prevInfoPointer = NULL;
		newRequest->nextInfoPointer = NULL;

		MPI_Num_requests++;
		MPI_First_request_pointer = newRequest;
		MPI_Last_request_pointer = newRequest;
	}
	else
	{
		newRequest->requestID = MPI_Last_request_pointer->requestID + 1;
		newRequest->prevInfoPointer = MPI_Last_request_pointer;
		newRequest->nextInfoPointer = NULL;

		MPI_Num_requests++;
		MPI_Last_request_pointer->nextInfoPointer = newRequest;
		MPI_Last_request_pointer = newRequest;
	}

	newRequest->type = type;
	newRequest->finished = 0;
	newRequest->targetFD = targetFD;
	newRequest->buf = buf;
	newRequest->count = count;
	newRequest->datatype = datatype;
	newRequest->other = other;
	newRequest->tag = tag;
	newRequest->comm = comm;

	return newRequest->requestID;
}

void DeleteRequestInfo(MPI_Request_info *request)
{
	if(MPI_Num_requests == 1)
	{
		free(request);
		
		MPI_Num_requests = 0;
		MPI_First_request_pointer = NULL;
		MPI_Last_request_pointer = NULL;
	}
	else
	{
		if(request->prevInfoPointer != NULL)
			request->prevInfoPointer->nextInfoPointer = request->nextInfoPointer;
		
		if(request->nextInfoPointer != NULL)
			request->nextInfoPointer->prevInfoPointer = request->prevInfoPointer;

		MPI_Num_requests--;
		
		if(MPI_First_request_pointer == request)
		{
			MPI_First_request_pointer = request->nextInfoPointer;
		}

		if(MPI_Last_request_pointer == request)
		{
			MPI_First_request_pointer = request->prevInfoPointer;
		}

		free(request);
	}
}
