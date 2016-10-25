/*
 * udpclient.c - socket UDP program thing
 * 
 * 
 * Copyright 2016 job <job@function1.nl>
 * 
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose is hereby granted.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA
 * OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <string.h>

#define BUFLEN 256

/* prints msg, with error details, and exits */
void ferr(const char* msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

/* gets a pointer to the internet address of a sockaddr* */
void * getinaddr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) /* IPv4 */
		return &(((struct sockaddr_in*)sa)->sin_addr);
		
	return &(((struct sockaddr_in6*)sa)->sin6_addr); /* IPv6 */
}

int main(int argc, char *argv[])
{
	int sock, rv;
	size_t msglen, n;
	struct addrinfo hints, *servinfo, *p;
	char buffer[BUFLEN];
	char ipbuffer[INET6_ADDRSTRLEN];
	char * sendbuffer = NULL;
	
	if (argc < 3)
	{
		fprintf(stderr, "Usage: %s <hostname> <port>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_UNSPEC; /*Unspecified(IPv4, IPv6, don't care)*/
	hints.ai_socktype = SOCK_DGRAM; /* TCP */
	
	/* get server info */
	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return EXIT_FAILURE;
	}
	
	/* browse through IP's */
	for (p = servinfo; p != NULL; p = p->ai_next)
	{
		inet_ntop(
			p->ai_family,
			getinaddr((struct sockaddr *)p->ai_addr),
			ipbuffer,
			sizeof(ipbuffer)
		);
		
		printf("Trying %s...", ipbuffer);
		fflush(stdout);
		
		if ((sock = socket(p->ai_family, p->ai_socktype,
			p->ai_protocol)) == -1)
		{
			perror("\tError opening socket");
			continue;
		}
		
		/* working socket found */
		printf("\tSuccess!\n");
		break;
	}
	
	if (p == NULL)
	{
		fprintf(stderr, "All attempts failed\n");
		exit(EXIT_FAILURE);
	}
	
	printf("Found %s! Say something!\n", ipbuffer);
	
	/*freeaddrinfo(servinfo);*/
	
	
	/* TODO: poll()? */
	int pid = fork();
	if (pid < 0)
	{
		ferr("Error on fork");
	}
	else if (pid == 0)
	{
		/* child */
		while (1)
		{
			n = getline(&sendbuffer, &msglen, stdin);
			
			if (n < 0)
				ferr("getline");
			
			n = sendto(sock, sendbuffer, strlen(sendbuffer), 0,
				p->ai_addr, p->ai_addrlen);
				
			if (n < 0)
				ferr("Error writing socket");
		}
	}
	else
	{
		/* parent */
		while (1)
		{
			memset(buffer, 0, BUFLEN);
			n = recvfrom(sock, buffer, BUFLEN - 1, 0,
				p->ai_addr, &p->ai_addrlen);
			if (n < 0)
				ferr("Error reading socket");
			else if (n == 0)
			{
				printf("Socket closed by server\n");
				close(sock);
				return 0;
			}
			printf("%s", buffer);
		}
		
	}
	
	close(sock);
	freeaddrinfo(servinfo);
	
	return 0;
}
