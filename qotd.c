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
	
	char ** list = malloc(1024 * sizeof(char *));
	
	char c;
	size_t i, j;
	char prevEsc = 0;
	
	/* TODO: What if more then 1024 quotes? */
	for (i = 0; ; i++)
	{
		list[i] = malloc(BUF_SIZE);
		
		for (j = 0; j < BUF_SIZE;)
		{
			c = fgetc(input);
			
			if (c == EOF)
			{
				j++;
				goto endoffile;
			}
			if (c == '\\' && !prevEsc)
			{
				prevEsc = 1;
				continue;
			}
			
			list[i][j] = c;
			
			if (c == '\n' && !prevEsc)
			{
				break;
			}
			
			j++;
			prevEsc = 0;
		}
		
		list[i] = realloc(list[i], j);
	}

endoffile:
	if (i == 0)
		ferr("No quotes in list!");

	list[i] = realloc(list[i], j);
	list = realloc(list, (i + 1) * sizeof(char *));
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
		
		r = sendto(sock, quotes[ran], strlen(quotes[ran]), 0, 
			(struct sockaddr *)&cli_addr, cli_len);
			
		if (r < 0)
			perror("sendto");
	}
		
	return 0;
}

