/*
 * qotd.c - IPv6 UPD QOTD server conforming to RFC865
 * 
 * usage: qotd [filename]
 * if filename is not given, quotes.txt will be tried
 * 
 * Copyright 2016-2018 job <job@function1.nl>
 * 
 * This code is released into the P U B L I C  D O M A I N!
 * 
 */


/* IDEAS:
	* comments
	* improve random distribution
 */

/* Set to 1 if you want to watch the quote file for changes, so
 * that when it changes the quote list is automatically updated.
 * This uses linux's inotify API, so it may not work in some
 * environments.
 * adding -D COMPILE_AUTOUPDATE to your compiler flags will typically
 * work as well */
#ifndef COMPILE_AUTOUPDATE
#define COMPILE_AUTOUPDATE 0
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>

#if COMPILE_AUTOUPDATE
	#include <sys/inotify.h>
	#include <poll.h>
	#include <errno.h>
#endif

#define BUF_SIZE 512

struct Quote
{
	char * msg;
	unsigned short len;
};

/* Converts a sockaddr to a human readable one */
char * get_ip_str(const struct sockaddr * sa, char * s, size_t maxlen)
{
	switch (sa->sa_family)
	{
		case AF_INET: /* IPv4 */
			inet_ntop(AF_INET, &(((struct sockaddr_in *)sa)->sin_addr),
				s, maxlen);
			break;
		
		case AF_INET6: /* IPv6 */
			inet_ntop(AF_INET6,
				&(((struct sockaddr_in6 *)sa)->sin6_addr),
				s, maxlen);
			break;
			
		default: /* Unknown */
			strncpy(s, "Unkown AF", maxlen);
			return NULL;
	}
	
	return s;
}

/* prints msg, with error details, and exits */
void ferr(const char * msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

/* Parses a quote list from a file. Returns NULL if there are no
 * quotes in file */
struct Quote * parse_quote_list(const char * filename, size_t * len)
{
	FILE * input = fopen(filename, "r");
	
	if (input == NULL)
	{
		/* I know that this should only be fatal for the first time,
		 * not when the quote file has been updated(fall back to old),
		 * but that seems to be a rare case and I am a little lazy. */
		ferr("Couldn't read quote list file");
	}
	
	/* initial list length, will double every time there is too little
	 * space */
	size_t list_len = 512;
	struct Quote * list = malloc(list_len * sizeof(struct Quote));
	
	struct Quote q; /* current quote */
	
	char * p = NULL; /* current quote message */
	int c; /* current unsigned char, cast to int for EOF */
	char prevEsc = 0; /* previous char was escape char */
	size_t i, j;
	
	for (i = 0; ; i++)
	{
		p = malloc(BUF_SIZE);
		
		/* -1 for terminating null byte */
		for (j = 0; j < BUF_SIZE - 1;)
		{
			c = fgetc(input);
			
			if (c == EOF)
			{
				goto endoffile;
			}
			if (c == '\\' && !prevEsc)
			{
				prevEsc = 1;
				continue;
			}

			p[j] = (char)c;
			j++;
			
			if (c == '\n' && !prevEsc)
			{
				/* if this is hit, high chance msg < BUF_SIZE, so
				 * resize */
				p = realloc(p, j);
				break;
			}
			
			prevEsc = 0;
		}
		
		q.msg = p;
		q.len = j;
		
		if (i >= list_len)
		{
			list_len *= 2;
			list = realloc(list, list_len * sizeof(struct Quote));
		}
		
		list[i] = q;
	}

endoffile:
	fclose(input);

	free(p);

	if (i == 0)
	{
		/* No quotes in list */
		free(list);
		return NULL;
	}

	/* if last quote has no \n quote won't be added */
	list = realloc(list, i * sizeof(struct Quote));
	*len = i;
	
	return list;
}

#if COMPILE_AUTOUPDATE
/* frees a quote list */
/* only used with autoupdate */
void free_quote_list(struct Quote * l, size_t len)
{
	for (size_t i = 0; i < len; i++)
	{
		free(l[i].msg);
	}
	free(l);
}
#endif

int main(int argc, char **argv)
{
	size_t n_quotes, ran;
	
	const struct in6_addr ia = IN6ADDR_ANY_INIT;
	
	int sock, r;
	struct sockaddr_in6 serv_addr;
	struct sockaddr_storage cli_addr;
	socklen_t cli_len = (socklen_t)sizeof(cli_addr);
	char buf[BUF_SIZE];
	char s[INET6_ADDRSTRLEN];

	#if COMPILE_AUTOUPDATE
	int watchfd; /* inotify watch file descriptor */
	int wd; /* quotefile watch descriptor */
	nfds_t fdlen = 2; /* number of poll() file descriptors(fds) */
	struct pollfd fdlist[2]; /* 2 fds: sock & wd */
	#endif

	char * quotepath = argc > 1 ? argv[1] : "quotes.txt";

	struct Quote * quotes = parse_quote_list(
		quotepath, &n_quotes
	);
	if (quotes == NULL)
	{
		fprintf(stderr, "No quotes in list!\n");
		exit(EXIT_FAILURE);
	}
	
	#if COMPILE_AUTOUPDATE
	/* create file descriptor for the inotify api,
	 * which watches the quote file */
	watchfd = inotify_init1(IN_NONBLOCK);
	
	if (watchfd < 0)
	{
		ferr("inotify_init1");
	}

	/* add watch descriptor for quote file */
	wd = inotify_add_watch(watchfd, quotepath, IN_CLOSE);
	if (wd < 0)
	{
		fprintf(stderr, "Cannot watch %s:", quotepath);
		ferr("inotify_add_watch");
	}
	#endif
	
	srand(time(NULL)); /* seed the RNG with the current time */
	
	
	sock = socket(AF_INET6,	SOCK_DGRAM, 0); /* IPv6 UDP socket */
	
	if (sock < 0)
		ferr("Error opening socket");
		
	const socklen_t yes = 1;
	
	/* make the socket reusable */
	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR,
		(void*)&yes, sizeof(yes)) < 0)
		ferr("Error on setsockopt");
		
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin6_family = AF_INET6; /* IPv6 */
	serv_addr.sin6_port = htons(17); /* spec specifies port 17*/
	serv_addr.sin6_addr = ia; /* use local ip */
		
	if (bind(sock, (struct sockaddr *)&serv_addr,
		sizeof(serv_addr)) < 0)
		ferr("Error on binding socket");
	
	#if COMPILE_AUTOUPDATE
	/* socket input */
	fdlist[0].fd = sock;
	fdlist[0].events = POLLIN;
	
	/* inotify quotefile */
	fdlist[1].fd = watchfd;
	fdlist[1].events = POLLIN;
	#endif
	
	while (1)
	{
		#if COMPILE_AUTOUPDATE
		r = poll(fdlist, fdlen, -1);
		if (r < 0)
		{
			/* EINTR is when there was an intteruption and the syscall
			 * fails. So do not fatal error, but try again! */
			if (errno == EINTR)
			{
				continue;
			}

			ferr("poll");
		}
		
		if (r == 0) continue;
		/* r > 0 from here */
		
		if (fdlist[0].revents & POLLIN)
		{
			/* socket data available */
		#endif
			memset(buf, 0, BUF_SIZE);
		
			r = recvfrom(sock, buf, BUF_SIZE - 1, 0, 
				(struct sockaddr *)&cli_addr, &cli_len);
			
			printf("Got message from %s\t%s", get_ip_str(
				(struct sockaddr *)&cli_addr, s, sizeof(s)), buf);
			
			ran = rand() % n_quotes; /* modulo bias XD */
			
			r = sendto(sock, quotes[ran].msg, quotes[ran].len, 0,
				(struct sockaddr *)&cli_addr, cli_len);
			
			if (r < 0)
				perror("Error on sendto");

		#if COMPILE_AUTOUPDATE
			continue;
		}
		
		if (fdlist[1].revents & POLLIN)
		{
			/* inotify event: quote list might be changed */
			
			/* Some systems cannot read integer variables if they 
			 * are not properly aligned. On other systems, incorrect
			 * alignment may decrease performance. Hence, the buffer
			 * used for reading from the inotify file descriptor
			 * should have the same alignment as
			 * struct inotify_event. (Taken from man inotify) */
			char ibuf[1024]
				__attribute__ ((aligned(
				__alignof__(struct inotify_event))));

			ssize_t ilen;
			struct inotify_event * ievent;
			char * p;
			
			/* browse through events */
			while (1)
			{
				ilen = read(watchfd, ibuf, sizeof(ibuf));
				
				if (ilen < 0 &&
					!(errno == EAGAIN || errno == EWOULDBLOCK))
				{
					/* EAGAIN/EWOULDBLOCK is when read() would
					 * block in order to read data(See `man read`)*/
					ferr("read");
				}
				
				if (ilen <= 0)
				{
					break;
				}
				
				for (p = ibuf; p < ibuf + ilen;
					p += sizeof(struct inotify_event))
				{
					ievent = (struct inotify_event *)p;
					
					if (ievent->mask & IN_CLOSE_WRITE)
					{
						/* might be changed! reparse quote list */
						printf("Quote list has been changed, installing new one...\n");
						
						size_t nq_len;
						struct Quote * nq = parse_quote_list(
							quotepath, &nq_len);
						if (nq == NULL)
						{
							printf("New quote list has no quotes! Abort.\n");
							break;
						}
						
						/* all good, update! */
						free_quote_list(quotes, n_quotes);
						quotes = nq;
						n_quotes = nq_len;
						
						/* break..? */
						break;
					}
					
					if (ievent->len)
					{
						/*printf("%s\n", ievent->name);*/
						p += ievent->len;
					}
				}
			}
			continue;
		}
		#endif
	}
	
	close(sock);
	#if COMPILE_AUTOUPDATE
	close(watchfd);
	#endif
		
	return 0;
}
