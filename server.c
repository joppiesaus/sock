/*
 * server.c - Creates a listening TCP socket that echo's messages to all
 *            its peers
 * 
 * Copyright 2016,2021 job <job@function1.nl>
 * 
 * CC0/Public Domain
 * 
 * IDEAS:
 *		Allow server to transmit messages as well
 * 		inform clients about changes
 * 		custom run-time max connection
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

#define INITIAL_CONN 24
#define MAX_CONN_ALLOC (8192 / sizeof(struct pollfd)) /* 8 KB max block size */
#define BACKLOG 5 /* listen() backlog */
#define BUFLEN 256 /* size of recv buffer */
#define TIMEOUT -1 /* indefinite poll timeout */

/* makes a for loop that skips INCR_VAR == SKIP_VAR efficiently */
#define FOR_LOOP_SKIP_N( INCR_VAR, INIT_VAL, SKIP_VAR, END_VAR, CODE_BLOCK ) \
	for ( (INCR_VAR) = (INIT_VAL); (INCR_VAR) < (SKIP_VAR); (INCR_VAR)++ ) {\
		CODE_BLOCK \
	}\
	(INCR_VAR)++; \
	for (; (INCR_VAR) < (END_VAR); (INCR_VAR)++ ) {\
		CODE_BLOCK \
	}


/* converts sockaddr* to string and puts into ipbuffer */
#define GETINET(s) inet_ntop(s->sa_family, getinaddr(s), ipbuffer, \
	sizeof(ipbuffer))

const char * RETURN_MESSAGE = "✓✓ seen\n";
const char * FULL_MESSAGE = "😩 I am sorry but the server is full\n";
	

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
	int sock, portno, rv, i, j, n, rn;
	struct sockaddr * serv_addr = NULL;
	struct sockaddr * cli_addr = NULL;
	socklen_t serv_addrlen = sizeof(struct sockaddr_in6);
	
	char buffer[BUFLEN];
	char ipbuffer[INET6_ADDRSTRLEN];
	
	int domain = AF_INET6;
	
	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s <port> [ipv4 or ipv6]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	if (argc > 2) {
		
		if ((argv[2][0] == '4' && argv[2][1] == 0x00) ||
			strncmp(argv[2], "ipv4", 4) == 0) {
			
			domain = AF_INET; /* IPv4 */
			serv_addrlen = sizeof(struct sockaddr_in);
			
			puts("Listening with IPv4");
			
		} else if (!(argv[2][0] == '6' && argv[2][1] == 0x00) &&
				strncmp(argv[2], "ipv6", 4) != 0){
			
			fprintf(stderr, "I have no idea what %s is, defaulting to IPv6\n", argv[2]);
		}
		
	}
	
	sock = socket(domain, SOCK_STREAM, 0); /* init TCP socket */
	
	if (sock < 0)
		ferr("Error opening socket");
		
	const socklen_t yes = 1;
	
	/* make the socket reusable */
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, 
		(void*)&yes, sizeof(yes)) < 0)
		perror("Error on setsockopt");
	
	/* TODO: Error handling */
	portno = atoi(argv[1]);
	
	serv_addr = malloc(serv_addrlen);
	memset(serv_addr, 0, serv_addrlen);
	serv_addr->sa_family = domain;
	
	if (domain == AF_INET6)
	{
		struct sockaddr_in6 * a = (struct sockaddr_in6 *)serv_addr;
		
		a->sin6_port = htons(portno);
		/* idk how C works exactly. Can't just assign it, because it's
		 * an array. In theory and practice this doesn't have to happen
		 * at all because it's just 128 0's, but I do this anyways
		 * might it be different one day. */
		memcpy(&a->sin6_addr.s6_addr, &in6addr_any, sizeof(in6addr_any));
	}
	else /* ipv4 */
	{
		struct sockaddr_in * a = (struct sockaddr_in *)serv_addr;
		
		a->sin_port = htons(portno);
		a->sin_addr.s_addr = INADDR_ANY;
	}
	
	
	
	if (bind(sock, (struct sockaddr *)serv_addr, serv_addrlen) < 0)
		ferr("Error on binding socket");
	
	if (listen(sock, BACKLOG) == -1)
		ferr("Error on listen");
	
	GETINET(serv_addr);
	printf("Success! Now listening on %s:%d...\n", ipbuffer, portno);
	
	socklen_t clilen = (socklen_t)serv_addrlen;
	cli_addr = malloc(clilen);
	
	
	int fdlen = INITIAL_CONN + 1;
	int sockcount = 1;
	
	struct pollfd * fdlist = malloc(fdlen * sizeof(struct pollfd));
	fdlist[0].fd = sock;
	fdlist[0].events = POLLIN;
	
	
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
			memset(cli_addr, 0, clilen);
			int cfd = accept(sock, cli_addr, &clilen);
			
			if (cfd < 0)
			{
				perror("Error accepting client");
				continue;
			}
			
			GETINET(cli_addr);
			printf("Incoming connection from %s... ", ipbuffer);
			fflush(stdout);
			
			if (sockcount >= fdlen)
			{
				/* resize array to fit the new connection */
				if (fdlen >= MAX_CONN_ALLOC) {
					fdlen += MAX_CONN_ALLOC;
				} else {
					fdlen *= 2;
				}
				
				/* people say: realloc can fail, so you must first
				 * create a new pointer and an old pointer and check for
				 * errors. Well, I reckon that if realloc fails, all
				 * hope is lost anyways. For important stuff, sure, but
				 * this program is not important stuff. */
				fdlist = realloc(fdlist, fdlen * sizeof(struct pollfd));
			}
			
			/* add it to the list of sockets */
			struct pollfd client;
			client.fd = cfd;
			client.events = POLLIN;
			
			fdlist[sockcount++] = client;
			
			printf("Accepted\n");
		}
		
		for (i = 1/*skip listening socket*/; i < sockcount; i++)
		{
			if (fdlist[i].revents & POLLIN)
			{
				/* incoming message! */
				memset(buffer, 0, BUFLEN);
				
				rn = recv(fdlist[i].fd, buffer, BUFLEN - 1, 0);
				
				if (rn < 0)
				{
					perror("Error reading socket");
					continue;
				}
				else if (rn == 0)
				{
					/* EOF detected, remove the socket */
					close(fdlist[i].fd);
					
					/* swap last element with the removed one */
					fdlist[i--] = fdlist[--sockcount];
					
					printf("Client left\n");
					
					/* resize of necessary */
					if (fdlen >= INITIAL_CONN * 2)
					{
						if (fdlen > MAX_CONN_ALLOC * 2 
								  + MAX_CONN_ALLOC / 2 &&
								fdlen - sockcount > MAX_CONN_ALLOC +
									MAX_CONN_ALLOC / 2)
						{
							fdlen -= MAX_CONN_ALLOC;
							fdlist = realloc(fdlist, fdlen *
								sizeof(struct pollfd));
						}
						else if (sockcount < fdlen / 2 - fdlen / 8)
						{
							fdlen /= 2;
							fdlist = realloc(fdlist, fdlen *
								sizeof(struct pollfd));
						}
					}
					
					continue;
				}
				
				printf("Received message: %s", buffer);
				
				/* send a kind message back to indicate that the message
				 * was received */
				n = send(fdlist[i].fd, RETURN_MESSAGE,
					strlen(RETURN_MESSAGE), 0);
					
				if (n < 0)
					perror("Error writing socket");
				
				
				/* send the message to the rest of the peers,
				 * and skip origin socket */
				FOR_LOOP_SKIP_N( j, 1, i, sockcount, {
					
					n = send(fdlist[j].fd, buffer, rn, 0);
					
					if (n < 0)
					{
						perror("Error writing socket");
						continue;
					}
				})
			}
		}
		
	}
	free(fdlist);
	free(serv_addr);
	
	return 0;
}

