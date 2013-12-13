#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>

#ifndef __USE_MISC
#define __USE_MISC
#endif
#include <arpa/inet.h>

#define ENABLE_SOCKET
//#define GNURADIO

int sock;
struct sockaddr_in addr;

int lte_dsock_init()
{
	sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (sock < 0) {
		fprintf(stderr, "Socket failed\n");
		return -1;
	}

	addr.sin_family = AF_INET;
	addr.sin_port = htons(8888);

	if (inet_aton("localhost", (struct in_addr *) &addr) < 0) {
		fprintf(stderr, "Address failed\n");
		return -1;
	}

	return 0;
}

int lte_dsock_send(float *data, int len, int chan)
{
#ifdef ENABLE_SOCKET
	int rc;
#ifndef GNURADIO
	uint8_t *hack = (uint8_t *) data;

	*hack = chan;
#else
	if (chan)
		return 0;
#endif
	rc = sendto(sock, data, 2 * len * sizeof(float), 0,
		    (const struct sockaddr *) &addr, sizeof(addr));
	if (rc < 0) {
		fprintf(stderr, "Send error\n");
		return -1;
	}
#endif
	return 0;
}

static void __attribute__((constructor)) init_sockets()
{
	lte_dsock_init();
}
