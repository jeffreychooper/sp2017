// TODO: need to adjust send and receive... make a linked list pointing to buffers where I store things to send and receive... works better with progress engine
// TODO: progress engine might need to be expanded... get info from it about whether who sent us what message
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
#define MIN_PORT 4500
#define MAX_PORT 4599
#define DEFAULT_BACKLOG 5
#define CONNECT_FLAG 1
#define QUIT_FLAG 2
#define BARRIER_FLAG 3
#define RECV_READY_FLAG 4
#define SEND_FLAG 5

int MPI_World_rank;
int MPI_World_size;
char *MPI_Control_host;
int MPI_Control_port;
char MPI_My_host[HOSTNAME_LENGTH];
int MPI_My_listen_port;

int MPI_Control_socket;
int MPI_My_accept_socket;
int *MPI_Rank_sockets;

void ErrorCheck(int val, char *str);
int SetupAcceptSocket();
int ProgressEngine(int blockingSocket);		// sometimes, we'll want to block on some certain socket... so we'll select on it in particular. will return 0 when successful

int MPI_Init(int *argc, char ***argv)
{
	setbuf(stdout, NULL);

	int rc;

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

	// validate args
	if(dest >= MPI_World_size)
		return MPI_ERR_RANK;

	// if(comm != MPI_COMMWORLD)
		// check if it's valid

	if(tag < 0 && tag != MPI_ANY_TAG)
		return MPI_ERR_TAG;

	// if no connection to the destination...
	if(MPI_Rank_sockets[dest] == -1)
	{
		// tell ppexec I need to connect to someone
		int connectFlag = CONNECT_FLAG;
		write(MPI_Control_socket, (void *)&connectFlag, sizeof(int));

		// tell ppexec which comm I'm talking about
		write(MPI_Control_socket, (void *)&comm, sizeof(int));

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
	}

	// tell dest we're sending
	int sendFlag = SEND_FLAG;
	write(MPI_Rank_sockets[dest], (void *)&sendFlag, sizeof(int));

	// tell dest our rank
	write(MPI_Rank_sockets[dest], (void *)&MPI_World_rank, sizeof(int));

	// tell dest the tag we're using
	write(MPI_Rank_sockets[dest], (void *)&tag, sizeof(int));

	// returns 0 when it's ready
	while(ProgressEngine(MPI_Rank_sockets[dest]))
		;

	int typeSize;

	if(datatype == MPI_CHAR)
		typeSize = sizeof(char);
	else if(datatype == MPI_INT)
		typeSize = sizeof(int);

	int bytesToWrite = count * typeSize;

	int bytesWritten = write(MPI_Rank_sockets[dest], buf, bytesToWrite);

	while(bytesWritten < bytesToWrite)
		bytesWritten += write(MPI_Rank_sockets[dest], buf + bytesWritten, bytesToWrite - bytesWritten);

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
		// if we don't have a connection yet, do the progress engine (which will do the accept socket work)
		while(MPI_Rank_sockets[source] == -1)
		{
			// returns 0 when it gets something from the specified socket
			while(ProgressEngine(MPI_My_accept_socket))
				;
		}

		// read from the sender until it tells me the tag it's sending
		int senderFlag;
		while(senderFlag != SEND_FLAG)
			read(MPI_Rank_sockets[source], (void *)&senderFlag, sizeof(int));

		int senderSource;
		read(MPI_Rank_sockets[source], (void *)&senderSource, sizeof(int));

		int senderTag;
		read(MPI_Rank_sockets[source], (void *)&senderTag, sizeof(int));

		status->MPI_SOURCE = senderSource;
		status->MPI_TAG = senderTag;

		if(tag == MPI_ANY_TAG || senderTag == tag)
		{
			int readyFlag = RECV_READY_FLAG;
			write(MPI_Rank_sockets[source], (void *)&readyFlag, sizeof(int));

			int typeSize;

			if(datatype == MPI_CHAR)
				typeSize = sizeof(char);
			else if(datatype == MPI_INT)
				typeSize = sizeof(int);

			int bytesToRead = count * typeSize;

			int bytesRead = read(MPI_Rank_sockets[source], buf, bytesToRead);

			while(bytesRead < bytesToRead)
				bytesRead += read(MPI_Rank_sockets[source], buf + bytesRead, bytesToRead - bytesRead);
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
		if(blockingSocket != -1)
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

					if(requestFlag == RECV_READY_FLAG)
					{
						return 0;
					}
				}
			}
		}
		else
		{
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
		}
	}

	// if we got this far, assume there was no specific task we're working on
	return 0;
}
