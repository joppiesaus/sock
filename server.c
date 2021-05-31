/*
 * server.c - Creates a listening socket that echo's messages to all
 *            its peers
 * 
 * Copyright 2016 job <job@function1.nl>
 * 
 * CC0/Public Domain
 * 
 * IDEAS:
 * 		IPv6 support
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <poll.h>

#define MAX_CONN 100
#define BACKLOG 5 /* listen() backlog */
#define BUFLEN 256 /* size of recv buffer */
#define TIMEOUT 30 * 1000 /* 30 seconds poll() timeout */

/* converts sockaddr* to string and puts into ipbuffer */
#define GETINET(s) inet_ntop(s->sa_family, getinaddr(s), ipbuffer, \
	sizeof(ipbuffer))

const char * RETURN_MESSAGE = "✓✓ seen\n";
const char * FULL_MESSAGE = "😩 I am sorry but the server is full";

/* prints msg, with error details, and exits */
void ferr(const char * msg)
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

int main(int argc, char **argv)
{
	int sock, portno, rv, i, j, n;
	struct sockaddr_in serv_addr;
	struct sockaddr * cli_addr = NULL;
	
	char buffer[BUFLEN];
	char ipbuffer[INET6_ADDRSTRLEN];
	
	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	/* TODO: IPv6 */
	sock = socket(AF_INET, SOCK_STREAM, 0); /* init TCP socket */
	
	if (sock < 0)
		ferr("Error opening socket");
		
	const socklen_t yes = 1;
	
	/* make the socket reusable */
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 
		(void*)&yes, sizeof(yes)) < 0)
		perror("Error on setsockopt");
	
	portno = atoi(argv[1]);
	
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	
	if (bind(sock, (struct sockaddr *)&serv_addr,
		sizeof(serv_addr)) < 0)
		ferr("Error on binding socket");
	
	if (listen(sock, BACKLOG) == -1)
		ferr("Error on listen");
		
	size_t cli_size = sizeof(serv_addr.sin_addr);
	socklen_t clilen = (socklen_t)cli_size;
	cli_addr = malloc(cli_size);
	
	/* TODO: make "infinite"? */
	struct pollfd fdlist[MAX_CONN + 1];
	fdlist[0].fd = sock;
	fdlist[0].events = POLLIN;
	
	int sockcount = 1;
	
	while (1)
	{
		/* poll for activity... */
		rv = poll(fdlist, (nfds_t)sockcount, TIMEOUT);
		
		if (rv == -1)
			ferr("poll() failed");
		else if (rv == 0)
		{
			/*printf("timeout\n");*/
			continue;
		}
		
		if (fdlist[0].revents & POLLIN)
		{
			/* new connection inbound */
			memset(cli_addr, 0, cli_size);
			int cfd = accept(sock, cli_addr, &clilen);
			
			if (cfd < 0)
			{
				perror("Error accepting client");
				continue;
			}
			
			GETINET(cli_addr);
			printf("Incoming connection from %s...\n", ipbuffer);
			
			if (sockcount > MAX_CONN)
			{
				/* server is full, reject */
				printf("Client rejected(server full)\n");
				
				/* send a kind message */
				n = send(cfd, FULL_MESSAGE, strlen(FULL_MESSAGE), 0);
				
				if (n < 0)
					perror("Failed to write server is full response");
					
				close(cfd);
				
				continue;
			}
			
			/* add it to the list of sockets */
			struct pollfd client;
			client.fd = cfd;
			client.events = POLLIN;
			
			fdlist[sockcount++] = client;
			
			printf("Accepted new client\n");
		}
		
		for (i = 1/*skip listening socket*/; i < sockcount; i++)
		{
			if (fdlist[i].revents & POLLIN)
			{
				/* incoming message! */
				memset(buffer, 0, BUFLEN);
				
				n = recv(fdlist[i].fd, buffer, BUFLEN - 1, 0);
				
				if (n < 0)
				{
					perror("Error reading socket");
					continue;
				}
				else if (n == 0)
				{
					/* EOF detected, remove the socket */
					close(fdlist[i].fd);
					
					/* swap last element with the removed one */
					fdlist[i--] = fdlist[--sockcount];
					
					printf("Client left\n");
					
					continue;
				}
				
				printf("Received message: %s", buffer);
				
				/* send a kind message back to indicate that the message
				 * was received */
				n = send(fdlist[i].fd, RETURN_MESSAGE,
					strlen(RETURN_MESSAGE), 0);
					
				if (n < 0)
					perror("Error writing socket");
				
				
				/* send the message to the rest of the peers */
				for (j = 1; j < sockcount; j++)
				{
					/* do not resend to sender */
					if (i == j) continue;
					
					n = send(fdlist[j].fd, buffer, strlen(buffer), 0);
					
					if (n < 0)
					{
						perror("Error writing socket");
						continue;
					}
				}
			}
		}
		
	}
	
	return 0;
}

