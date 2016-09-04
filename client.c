/*
 * client.c
 * 
 * Copyright 2016 job <job@COMMUNICATE>
 * 
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301, USA.
 * 
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
#include <strings.h>

#define BUFLEN 256

void ferr(const char* msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

// Gets a pointer to the internet address of a sockaddr*
void * getinaddr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) // IPv4
		return &(((struct sockaddr_in*)sa)->sin_addr);
		
	return &(((struct sockaddr_in6*)sa)->sin6_addr); // IPv6
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
	hints.ai_family = AF_UNSPEC; // Unspecified(IPv4, IPv6, don't care)
	hints.ai_socktype = SOCK_STREAM; // TCP
	
	// get server info
	if ((rv = getaddrinfo(argv[1], argv[2], &hints, &servinfo)) != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return EXIT_FAILURE;
	}
	
	// browse through IP's
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
		
		if (connect(sock, p->ai_addr, p->ai_addrlen) == -1)
		{
			perror("\tFailed");
			close(sock);
			continue;
		}
		
		// working socket found
		printf("\tSuccess!\n");
		break;
	}
	
	if (p == NULL)
	{
		fprintf(stderr, "All attempts failed\n");
		return EXIT_FAILURE;
	}
	
	printf("Connected to %s! Say something!\n", ipbuffer);
	
	freeaddrinfo(servinfo);
	
	
	// TODO: poll()?
	int pid = fork();
	if (pid < 0)
	{
		ferr("Error on fork");
	}
	else if (pid == 0)
	{
		// child
		while (1)
		{
			n = getline(&sendbuffer, &msglen, stdin);
			
			if (n < 0)
				ferr("getline");
			
			n = send(sock, sendbuffer, strlen(sendbuffer), 0);
			if (n < 0)
				ferr("Error writing socket");
		}
	}
	else
	{
		// parent
		while (1)
		{
			memset(buffer, 0, BUFLEN);
			n = recv(sock, buffer, BUFLEN - 1, 0);
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
	
	return 0;
}

