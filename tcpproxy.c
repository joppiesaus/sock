/*
 * tcpproxy.c - A simple TCP proxy
 * 
 * made in 2021 by job <job@function1.nl>
 * 
 * CC0/Public domain
 * 
 * IDEAS: 
 * 		print addresses and such
 * 		sock_nonblock?
 */


#include <stdio.h>
#include <stdint.h>
#include <stdlib.h> /* atoi */
#include <string.h> /* memset */

/* socket stuff */
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h> /* getaddrinfo */
#include <unistd.h> /* close */
#include <arpa/inet.h> /* htons, inet_ntop */

#include <poll.h>
#include <errno.h>


#define BUF_LEN 512

void * getinaddr(struct sockaddr * sa)
{
	if (sa->sa_family == AF_INET) /* IPv4 */
		return &(((struct sockaddr_in*)sa)->sin_addr);
		
	return &(((struct sockaddr_in6*)sa)->sin6_addr); /* IPv6 */
}



int main(int argc, char **argv)
{
	int domain = AF_INET6;
	
	#define IS_IPV6 (domain == AF_INET6)
	
	int r;
	int listen_port;
	/* l is for listen, t is for target*/
	int lsock, tsock;
	struct addrinfo hints, *servinfo, *p;
	struct sockaddr * listen_addr;
	struct sockaddr * n_addr;
	socklen_t listen_addrlen = sizeof(struct sockaddr_in6);
	socklen_t n_addrlen;
	
	
	
	if (argc < 4) {
		fprintf(stdout, "Usage: %s <listen port> <target addr> <target port> [6 or 4 for IPv6 or IPv4]\n", argv[0]);
		return 1;
	}
	
	listen_port = atoi(argv[1]);
	
	if (argc >= 5) {
		if ((argv[4][0] == '4' && argv[4][1] == 0x00) ||
			strncmp(argv[4], "ipv4", 4) == 0) {
			
			domain = AF_INET; /* IPv4 */
			listen_addrlen = sizeof(struct sockaddr_in);
			puts("Connecting via IPv4");
		} else {
			fprintf(stderr, "I have no idea what %s is, defaulting to IPv6\n", argv[4]);
		}
	}
	
	
	/* Creating listen socket */
	lsock = socket(domain, SOCK_STREAM, 0); /* TCP socket */
	
	if (lsock == -1) {
		perror("could not open listen socket");
		return 1;
	}
	
	
	{
		/* this is in brackets so that petsky yes isn't referencable
		 * outside this scope */
		const socklen_t yes = 1;
		
		/* Make socket reusable */
		if (setsockopt(lsock, SOL_SOCKET, SO_REUSEADDR,
				&yes, sizeof(yes)) < 0) {
			perror("setsockopt");
			return 1;
		}
	}
	
	listen_addr = malloc(listen_addrlen);
	memset(listen_addr, 0, listen_addrlen);
	
	if (domain == AF_INET6)
	{
		struct sockaddr_in6 * a = (struct sockaddr_in6 *)listen_addr;
		
		a->sin6_family = AF_INET6; /* IPv6 */
		a->sin6_port = htons(listen_port);
		/* No need to set this, as we've set the entire struct to 0,
		 * and as IN6ADDR_ANY_INIT is a bunch of zeroes, this is
		 * uneccesarry. */
		/*a->sin6_addr = any; */  /* local address */
	}
	else
	{
		struct sockaddr_in * a = (struct sockaddr_in *)listen_addr;
		
		a->sin_family = AF_INET;
		a->sin_port = htons(listen_port);
		/*a->sin_addr = any;*/
	}
	
	if (bind(lsock, listen_addr, listen_addrlen) < 0) {
		perror("bind");
		return 1;
	}
	
	r = listen(lsock, 1);
	
	if (r == -1) {
		perror("listen");
		return 1;
	}
	
	
	/* Connecting to target */
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = domain;
	hints.ai_socktype = SOCK_STREAM; /* TCP */
	
	r = getaddrinfo(argv[2], argv[3], &hints, &servinfo);
	if (r != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(r));
		return 1;
	}
	
	/* loop through possible socket candidates */
	for (p = servinfo; p != NULL; p = p->ai_next) {
		
		tsock = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if (tsock == -1)
			continue;
		
		if (connect(tsock, p->ai_addr, p->ai_addrlen) == -1)
		{
			perror("connect");
			close(tsock);
			continue;
		}
		
		break;
	}
	
	if (p == NULL) {
		fprintf(stderr, "All attempts to connect to target host have failed\n");
		return 1;
	}
	
	puts("Connected! Listening...");
	
	/* TODO: SOCK NONBLOCK */
	
	n_addrlen = p->ai_addrlen;
	n_addr = malloc(p->ai_addrlen);
	
	int nsock = accept(lsock, n_addr, &n_addrlen);
	
	if (nsock == -1) {
		perror("accept");
		return 1;
	}
	
	/* pretty print the addresses */
	int ntop_buf_len = IS_IPV6 ? INET6_ADDRSTRLEN : INET_ADDRSTRLEN;
	
	char * ntop_buf_a = malloc(ntop_buf_len);
	char * ntop_buf_b = malloc(ntop_buf_len);
	
	/* took me a longer time I would like to admit to resolve a segfault,
	 * turns out you have to initialize n_addr during accept yourself... */
	if (inet_ntop(domain, getinaddr(n_addr), ntop_buf_a, ntop_buf_len) == NULL ||
		inet_ntop(domain, getinaddr(p->ai_addr), ntop_buf_b, ntop_buf_len) == NULL) {
		perror("inet_ntop");
	} else {
		printf("%s <-> %s\n", ntop_buf_a, ntop_buf_b);
	}
	
	free(ntop_buf_a);
	free(ntop_buf_b);
	
	
	
	/* the magic number 2 is the length of the fd list, if you see the
	 * magic number 2 than it is the length of this array */
	struct pollfd fds[2];
	
	fds[0].fd = nsock;
	fds[0].events = POLLIN;
	
	fds[1].fd = tsock;
	fds[1].events = POLLIN;
	
	int b;
	uint8_t buf[BUF_LEN];
	
	
	while (1)
	{
		r = poll(fds, 2, -1);
		if (r < 0)
		{
			/* EINTR is interrupt, non-fatal error, try again */
			if (errno == EINTR)
				continue;
			
			perror("poll");
			return 1;
		}
		
		if (r == 0) continue;
		
		for (int i = 0; i < 2; i++) {
			
			/* no data? skip */
			if (!(fds[i].revents & POLLIN)) continue;
			
			memset(&buf, 0, BUF_LEN);
			
			b = recv(fds[i].fd, (void *)&buf, BUF_LEN, 0);
			
			if (b < 0) {
				perror("recv");
				return 1;
			}
			else if (b == 0) {
				/* EOF */
				printf("EOF(probably connection %d closed), bye\n", i);
				
				goto quit; /* hah i am using goto anyways */
			}
			
			/* TODO: specify IP address */
			printf("message from %d: length %d\n", i, b);
			
			puts((const char*)buf);
			
			
			/* send the data to the next socket */
			b = send(fds[(i + 1) % 2].fd, &buf, b, 0);
			
			if (b < 0) {
				perror("send");
				return 1;
			}
		}
	}
	
quit:
	close(nsock);
	close(tsock);
	close(lsock);
	freeaddrinfo(p);
	free(listen_addr);
	free(n_addr);
	
	return 0;
}
