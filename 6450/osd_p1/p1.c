#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <fcntl.h>

#define MAX_PACKET_SIZE 1006
#define END 		0300
#define ESC 		0333
#define ESC_END 	0334
#define ESC_ESC		0335

int sendSockFD;
int recvSockFD;

void SendChar(unsigned char c)
{
	write(sendSockFD, &c, 1);
}

int RecvChar(unsigned char *c)
{
	int byteReceived = read(recvSockFD, c, 1);

	return byteReceived;
}

void SendPacket(unsigned char *p, int len)
{
	SendChar(END);

	while(len--)
	{
		switch(*p)
		{
			case END:
				SendChar(ESC);
				printf("An ESC was sent!\n");
				SendChar(ESC_END);
				printf("An ESC_END was sent!\n");
				break;
			case ESC:
				SendChar(ESC);
				printf("An ESC was sent!\n");
				SendChar(ESC_ESC);
				printf("An ESC_ESC was sent!\n");
				break;
			default:
				SendChar(*p);
				break;
		}

		p++;
	}

	SendChar(END);
}

int RecvPacket(unsigned char *p, int len)
{
	unsigned char c;
	int received = 0;
	int didRecvChar = 0;

	while(1)
	{
		didRecvChar = RecvChar(&c);

		if(!didRecvChar)
			return received;

		switch(c)
		{
			case END:
				if(received)
					return received;
				else
					break;
			case ESC:
				didRecvChar = RecvChar(&c);

				switch(c)
				{
					case ESC_END:
						c = END;
						break;
					case ESC_ESC:
						c = ESC;
						break;
				}
			default:
				if(received < len)
					p[received++] = c;
				break;
		}
	}
}

int main(int argc, char *argv[])
{
	// get a sockpair
	int sockpair[2];

	socketpair(AF_UNIX, SOCK_STREAM, 0, sockpair);

	// start a second process
	int forkRC = fork();
	
	if(forkRC > 0)
	{
		sendSockFD = sockpair[1];
		close(sockpair[0]);

		// open up infile
		int infileFD = open("infile", O_RDONLY);

		// read from infile and write contents to socket using SendPacket
		unsigned char buffer[MAX_PACKET_SIZE];
		int bytesRead = read(infileFD, buffer, MAX_PACKET_SIZE);

		while(bytesRead)
		{
			SendPacket(buffer, bytesRead);
			bytesRead = read(infileFD, buffer, MAX_PACKET_SIZE);
		}

		close(infileFD);
		close(sendSockFD);
	}
	else
	{
		recvSockFD = sockpair[0];
		close(sockpair[1]);

		// open up outfile
		int outfileFD = open("outfile", O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR);

		// read from socket using RecvPacket and write to outfile
		unsigned char buffer[MAX_PACKET_SIZE];
		int bytesReceived = RecvPacket(buffer, MAX_PACKET_SIZE);

		while(bytesReceived)
		{
			int bytesWritten = write(outfileFD, buffer, bytesReceived);

			while(bytesWritten < bytesReceived)
				bytesWritten += write(outfileFD, buffer + bytesWritten, bytesReceived - bytesWritten);

			bytesReceived = RecvPacket(buffer, MAX_PACKET_SIZE);
		}

		close(outfileFD);
		close(recvSockFD);

		exit(0);
	}

	return 0;
}
