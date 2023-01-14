#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#define MAXLINE 4096
#define MAXWSP 126
#define PORT 9999
#define IP "43.142.118.149"
#define PATH "/chat-ws"

/*
# http_shake: Upgrade protocols
Arguments:
	| fd: socket
	| sendbuf: send buff area
Results:
	| "> 0": successful, return strlen(sendbuf)
	| -1: failed, return -1
*/
int http_shake(int fd, char *sendbuf);

/*
# create_socket: *
Arguments:
	| *fd: int pointer to save socket
Results:
	| 1: successful
	| -1: failed
*/
int create_socket(int *fd);
/*
# ws_send: send data
Arguments:
	| *fd: int pointer to save socket
	| *data: text data
	| *sendbuf: send buff
	| size: length of data
Results:
	| "> 1": size
	| -1: failed
*/
int ws_send(int fd, char *data, char *sendbuf, int size);

int main() 
{	
	int fd;
	char sendbuf[MAXLINE], recvbuf[MAXLINE], data[MAXWSP];

	create_socket(&fd);
	printf("create socket...\n");

	// HTTP Shake
	http_shake(fd, sendbuf);
	printf("HTTP shake...\n");
	memset(sendbuf, 0, MAXLINE);

	// Receive HTTP Upgrade response
	recv(fd, recvbuf, MAXLINE, 0);
	memset(recvbuf, 0, MAXLINE);

	// Join
	strcat(data, "{\"cmd\":\"join\"," \
		"\"channel\":\"lounge\",\"nick\":\"CBoot\"}");
	ws_send(fd, data, sendbuf, strlen(data) + 6);
	printf("send join message...\n");
	memset(sendbuf, 0, MAXLINE);
	memset(data, 0, MAXWSP);

	// Chat
	strcat(data, "{\"cmd\":\"chat\"," \
		"\"text\":\"Hi, I'm a C bot\"}");
	ws_send(fd, data, sendbuf, strlen(data) + 6);
	printf("send join message...\n");
	memset(sendbuf, 0, MAXLINE);
	memset(data, 0, MAXWSP);

	// Listen
	for (;;)
	{
		memset(recvbuf, 0, MAXLINE);
		if (recv(fd, recvbuf, MAXLINE, 0) > 0)
		{
			printf("Recv: %s\n", recvbuf);
			continue;
		}
		else
		{
			printf("error when receiving...\n");
			break;
		}
	}

	// Exit
	close(fd);
	exit(EXIT_SUCCESS);

	return 0;
}

int create_socket(int *fd)
{
	struct sockaddr_in addr;
	*fd = socket(AF_INET, SOCK_STREAM, 0);

	if (*fd < 0)
	{
		return -1;
	}

	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;
	addr.sin_port = htons(PORT);

	if (inet_pton(AF_INET, IP, &addr.sin_addr) <= 0)
	{
		return -1;
	}

	if(connect(*fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		return -1;
	}

	return 1;
}

int http_shake(int fd, char *sendbuf)
{
	strcat(sendbuf, "GET ");
	strcat(sendbuf, PATH);
	strcat(sendbuf, " HTTP/1.1\r\n"); 
	strcat(sendbuf, "Connection: Upgrade\r\n");
	strcat(sendbuf, "Upgrade: websocket\r\n");
	strcat(sendbuf, "Sec-WebSocket-Version: 13\r\n");
	strcat(sendbuf, "Sec-WebSocket-Key: WI8CnBeifs5DmMHEdR1JmA==\r\n");
	strcat(sendbuf, "User-Agent: FuckYourMotherFuckshit\r\n\r\n");
	if (send(fd, sendbuf, strlen(sendbuf), 0) < 0)
	{
		return -1;
	}
	return strlen(sendbuf);
}

int ws_send(int fd, char *data, char *sendbuf, int size)
{
	sendbuf[0] = (uint8_t)((1 << 7) + 0x01); // opcode: 0x01
	sendbuf[1] = (1 << 7) + (size - 6);
	// Masking key
	int maskingkey[4] = {0x11, 0x45, 0x14, 0x19};
	sendbuf[2] = maskingkey[0];
	sendbuf[3] = maskingkey[1];
	sendbuf[4] = maskingkey[2];
	sendbuf[5] = maskingkey[3];
	for (int i = 0; i < strlen(data); i++)
	{
		sendbuf[(6 + i)] = maskingkey[(i % 4)] ^ (int)(data[i]);
	}

	if (send(fd, sendbuf, size, 0) < 0)
	{
		return -1;
	}
	return size;
}
