/*
 * icmp6mys.c
 * 
 * does something
 * 
 * Copyright 2017 job <job@COMMUNICATE>
 * 
 * just do whatever you want okay
 * 
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <stdint.h> /* uint16_t */
#include <sys/types.h>
#include <sys/socket.h> /* socket() */
#include <netinet/in.h> /* IP stuff */
#include <netdb.h> /* getaddrinfo() */
#include <arpa/inet.h> /* inet_ntop() */
#include <netinet/icmp6.h> /* icmp6_hdr and other ICPMv6 stuff */

#define MAX_PACKET_LEN 2048


void ferr(const char* msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

void print_icmp6hdrinfo(const struct icmp6_hdr * hdr)
{
	printf("type:\t%d\t(", hdr->icmp6_type);
	if (hdr->icmp6_type & 0x80 /* info mask */)
	{
		printf("Info: ");
	}
	else
	{
		printf("Error: ");
	}
	switch (hdr->icmp6_type)
	{
		/* I hope these headers are portable */
		
		/* ICPMv6 Errors */
		case ICMP6_DST_UNREACH: /* 1 */
			printf("Destination Unreachable");
			break;
			
		case ICMP6_PACKET_TOO_BIG: /* 2 */
			printf("Packet too big");
			break;
			
		case ICMP6_TIME_EXCEEDED: /* 3 */
			printf("Time Exceeded");
			break;
			
		case ICMP6_PARAM_PROB: /* 4 */
			printf("Parameter problem");
			break;
		
		/* ICMPv6 Info */
		case ICMP6_ECHO_REQUEST: /* 128 */
			printf("Echo request");
			break;
			
		case ICMP6_ECHO_REPLY: /* 129 */
			printf("Echo reply");
			break;
		
		/* Neighbor Discovery Protocol */
		case ND_ROUTER_SOLICIT: /* 133 */
			printf("NDP Router solicitation");
			break;
			
		case ND_ROUTER_ADVERT: /* 134 */
			printf("NDP Router advertisement");
			break;
		
		case ND_NEIGHBOR_SOLICIT: /* 135 */
			printf("NDP Neighbor solicitation");
			break;
			
		case ND_NEIGHBOR_ADVERT: /* 136 */
			printf("NDP Neighbor advertisement");
			break;
		
		case ND_REDIRECT: /* 137 */
			printf("NDP Redirect message");
			break;
			
		default:
			printf("Other");
			break;
	}
	printf(")\n");
	printf("code:\t%d\n", hdr->icmp6_code);
	printf("cksum:\t0x%04x\n", hdr->icmp6_cksum);
	printf("id:\t%d\n", ntohs(hdr->icmp6_id));
	printf("seq:\t%d\n", ntohs(hdr->icmp6_seq));
}

/* gets a pointer to the internet address of a sockaddr* */
void * getinaddr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) /* IPv4 */
		return &(((struct sockaddr_in*)sa)->sin_addr);
		
	return &(((struct sockaddr_in6*)sa)->sin6_addr); /* IPv6 */
}

/* Computes the standard internet checksum (RFC 1071) */
/* Using stupid types because I don't know if I can trust the compiler */
uint16_t checksum(uint16_t * b, size_t len)
{
	/*register*/ uint32_t sum = 0;
	
	/* Sum pairs of bytes until none or one byte left */
	while (len > 1)
	{
		sum += *(b++);
		len -= 2;
	}
	
	/* Add left-over byte, if any */
	if (len > 0)
		sum += *((uint8_t *)b);


#if 1 == 1
	/* Fold the 4 byte sum into 2 bytes
	   sum = lower 2 bytes + upper 2 bytes */
	sum = (sum >> 0x10) + (sum & 0xffff);
	/* Add carry, if any */
	sum += (sum >> 0x10);
#else
	/* Another way of writing this: */

	while (sum >> 0x10) {
		sum = (sum & 0xffff) + (sum >> 0x10);
	}
#endif
	
	/* ~ is bitwise NOT, or the one's complement */
	return (uint16_t)(~sum);
}


int main(int argc, char **argv)
{
	/* TODO: Set by program args */
	int ttl = 0; /* IPv6 time to live */
	
	if (argc < 2)
	{
		printf("Usage: %s <hostname>\n", argv[0]);
		return EXIT_FAILURE;
	}
	
	
	int sock, rv;
	struct addrinfo hints, *target_info, *p;
	struct sockaddr * resp_addr;
	socklen_t resp_addrlen = sizeof(struct sockaddr_in6);
	char ipbuffer[INET6_ADDRSTRLEN];
	
	
	/* Give getaddrinfo some hints: A raw IPv6 socket for ICMP6! */
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_ICMPV6;
	
	rv = getaddrinfo(argv[1], NULL, &hints, &target_info);
	
	if (rv != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		exit(EXIT_FAILURE);
	}
	
	/* Loop through possibilities */
	for (p = target_info; p != NULL; p = target_info->ai_next)
	{	
		/* Print out IP */	
		inet_ntop(
			p->ai_family,
			/* It should be IPv6 */
			/*&(((struct sockaddr_in6 *)p->ai_addr)->sin6_addr),*/
			getinaddr((struct sockaddr *)p->ai_addr),
			ipbuffer,
			sizeof(ipbuffer)
		);
		
		printf("Trying %s... ", ipbuffer);
		fflush(stdout);
		
		/* Try to create a socket */
		sock = socket(p->ai_family, p->ai_socktype,
			p->ai_protocol);
			
		/* TODO: SOCK_NONBLOCK */
			
		if (sock < 0)
		{
			perror("socket failed");
			continue;
		}
		
		printf("Success!\n");
		break;
	}
	
	if (p == NULL)
	{
		fprintf(stderr, "All attempts failed\n");
		exit(EXIT_FAILURE);
	}
	
	
	if (ttl > 0)
	{
		if (setsockopt(sock, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
				&ttl, sizeof(ttl)) < 0)
		{
			perror("setsockopt failed setting IPV6_UNICAST HOPS");
		}
	}
	
	/* TODO: Do tha random id stuff..? */
	short id = 12345;
	struct icmp6_hdr hdr;
	int r;
	
	int pid = fork();
	if (pid < 0)
	{
		ferr("fork");
	}
	else if (pid == 0)
	{
		/* child */
		resp_addr = malloc(resp_addrlen);
		
		char packet[MAX_PACKET_LEN];
		memset(packet, 0, MAX_PACKET_LEN);
		
		char * buf = NULL;
		size_t buf_size;
			
		/* TODO: Receiving proper messages */
		while (1)
		{
			r = recvfrom(
				sock,
				packet,
				MAX_PACKET_LEN,
				0,
				resp_addr,
				&resp_addrlen
			);
			
			if (r < 0)
			{
				perror("recvfrom");
				continue;
			}	
			
			inet_ntop(
				p->ai_family, /* !!! UNSAFE AND RETARDED !!! */
				getinaddr(resp_addr),
				ipbuffer,
				sizeof(ipbuffer)
			);
			
			memcpy(&hdr, packet, sizeof(hdr));
			

			/* Check if error or info */
			if (hdr.icmp6_type & 0x80 /* info mask */)
			{
				/* TODO: Check if resp_addr == p->ai_addr */
				
				/* info */
				if (hdr.icmp6_type == ICMP6_ECHO_REQUEST /*128*/)
				{
					printf("Incoming message!\n");
				}
				else if (hdr.icmp6_type == ICMP6_ECHO_REPLY /*129*/)
				{
					if (ntohs(hdr.icmp6_id) == id)
					{					
						printf("Message acknowledged!\n");	
					}
					/*continue;*/
				}
				else
				{
					/* Uninteresting router solicitations */
					continue;
				}
			}
			
			printf("Got packet from %s\n", ipbuffer);
			print_icmp6hdrinfo(&hdr);
			
			
			buf_size = r - sizeof(hdr);
			
			buf = realloc(buf, buf_size);
			
			memcpy(buf, packet + sizeof(hdr), buf_size);
			
			printf("len: %d, msg: %s\n", r, buf);
			
			/* Reset the buffer so the string terminates properly 
			 * the next time */
			memset(buf, 0, buf_size);
		}
		
		free(resp_addr);
		if (buf_size > 0) free(buf);
		
		return 0;
	}
	else
	{
		/* parent */
		size_t msg_len, msg_alloc_len, packet_len;
		char * msg = NULL;
		char * packet = NULL;
		short seq = 1;
		
		while (1)
		{
			/* Craft ICMPv6 header */
			memset(&hdr, 0, sizeof(hdr));
			hdr.icmp6_type = ICMP6_ECHO_REQUEST;
			hdr.icmp6_code = 0;
			hdr.icmp6_cksum = 0; /* Will be calculated by the IP stack */
			hdr.icmp6_id = htons(id);
			hdr.icmp6_seq = htons(seq);
			
			r = getline(&msg, &msg_alloc_len, stdin);
			
			if (r < 0)
				ferr("getline");
				
			msg_len = (size_t)r;
			
			print_icmp6hdrinfo(&hdr);
				
			/* I think the max packet length of sendto is around 0xffff */
			packet_len = sizeof(hdr) + msg_len;

			packet = realloc(packet, packet_len);

			memcpy(packet, &hdr, sizeof(hdr));
			memcpy(packet + sizeof(hdr), msg, msg_len);
			
			r = sendto(
				sock,
				packet,
				packet_len,
				0,
				p->ai_addr,
				p->ai_addrlen
			);

			if (r < 0)
				perror("sendto");
				
			printf("Sent %d bytes(msglen: %zu, hdrsize: %zu)\n",
				r, msg_len, sizeof(hdr));
				
			seq++;
		}
		
		free(packet);
		/* According to man getline it should be freed */
		/*if (msg_alloc_len > 0)*/ free(msg);
	}
	
	freeaddrinfo(target_info);
	close(sock);
	
	return 0;
}
