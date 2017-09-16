/*
 * qotd.c - IPv6 UPD QOTD server conforming to RFC865
 * 
 * Copyright 2016 job <job@function1.nl>
 * 
 * This code is released into the P U B L I C  D O M A I N!
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>

#define BUF_SIZE 512

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

char ** parse_quote_list(const char * filename, size_t * len)
{
	FILE * input = fopen(filename, "r");
	
	if (input == NULL)
		ferr("Couldn't read quote list file");
	
	/* initial list length, will double every time there is too little
	 * space */
	size_t list_len = 512;
	char ** list = malloc(list_len * sizeof(char *));
	
	char * p = NULL; /* current quote */
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
			
			/* TODO: is this possible because of cast int to char? */
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
				p = realloc(p, j + 1);
				break;
			}
			
			prevEsc = 0;
		}
		
		/* Add terminating null byte */
		p[j] = 0x00;
		
		if (i >= list_len)
		{
			list_len *= 2;
			list = realloc(list, list_len * sizeof(char *));
		}
		
		list[i] = p;
	}

endoffile:
	fclose(input);

	if (i == 0)
		ferr("No quotes in list!");

	/* if last quote has no \n quote won't be added */
	free(p);
	list = realloc(list, i * sizeof(char *));
	*len = i;
	
	return list;
}


int main(int argc, char **argv)
{
	size_t n_quotes;
	char ** quotes = parse_quote_list("quotes.txt", &n_quotes);
	
	srand(time(NULL)); /* seed the RNG with the current time */
		
	const struct in6_addr ia = IN6ADDR_ANY_INIT;
	
	int sock, r;
	struct sockaddr_in6 serv_addr;
	struct sockaddr_storage cli_addr;
	socklen_t cli_len = (socklen_t)sizeof(cli_addr);
	char buf[BUF_SIZE];
	char s[INET6_ADDRSTRLEN];
	
	
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
	
	while (1)
	{
		memset(buf, 0, BUF_SIZE);
		
		r = recvfrom(sock, buf, BUF_SIZE - 1, 0, 
			(struct sockaddr *)&cli_addr, &cli_len);
		
		printf("Got message from %s\t%s", get_ip_str(
			(struct sockaddr *)&cli_addr, s, sizeof(s)), buf);
		
		size_t ran = rand() % n_quotes; /* modulo bias XD */
		
		/* TODO: Maybe struct with len built in instead of strlen? */
		r = sendto(sock, quotes[ran], strlen(quotes[ran]), 0, 
			(struct sockaddr *)&cli_addr, cli_len);
			
		if (r < 0)
			perror("Error on sendto");
	}
		
	return 0;
}
