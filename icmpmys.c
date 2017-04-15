/*
 * icmpmys.c - Mysererious and inconvenient application to chat over
 *             the internet using ICMPv6 over IPv6 or ICMP over IPv4
 * 
 * 
 * Copyright 2017 job <job@function1.nl>
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

/* Define IPv4 to anything to compile IPv4 ICMP, otherwise it will
 * compile IPv6 ICMPv6.
 * This compiler flag will typically work: cc -D IPV4
 * Uncommenting the following line will also work: */
/*#define IPV4 1*/


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include <stdint.h> /* uint16_t */
#include <sys/types.h>
#include <sys/socket.h> /* socket() */
#include <netinet/in.h> /* IP_MAXPACKET, other IP stuff */
#include <netdb.h> /* getaddrinfo() */
#include <arpa/inet.h> /* inet_ntop() */

#if IPV4
	#include <netinet/ip_icmp.h> /* struct icmp */
#else
	#include <netinet/icmp6.h> /* icmp6_hdr and other ICPMv6 stuff */
#endif

#define MAX_PACKET_LEN 2048


void ferr(const char* msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

/* This defines an abstract function that prints info about a
 * ICMP/ICMPv6 header based on the IPV4 define */
#if IPV4
void print_icmphdrinfo(const struct icmp * hdr)
{
	/* IPv4 */
	printf("type:\t%d\t(", hdr->icmp_type);
	
	#define ERR printf("Error: ")
	#define INF printf("Info: ")
	
	switch (hdr->icmp_type)
	{
		/* Really hope these headers are also portable */
		
		/* Errors */
		case ICMP_DEST_UNREACH: /* 3 */
			ERR;
			printf("Destination Unreachable");
			break;
			
		case ICMP_SOURCE_QUENCH: /* deprecated, 4 */
			ERR;
			printf("Source quench");
			break;
		
		case ICMP_TIME_EXCEEDED: /* 11 */
			ERR;
			printf("Time exceeded");
			break;
		
		/* Info */
		case ICMP_ECHOREPLY: /* 0 */
			INF;
			printf("Echo reply");
			break;
		
		case ICMP_ECHO: /* 8 */
			INF;
			printf("Echo request");
			break;
			
		default:
			printf("Other");
			break;
	}
	printf(")\n");
	printf("code:\t%d\n", hdr->icmp_code);
	printf("cksum:\t0x%04x\n", hdr->icmp_cksum);
	printf("id:\t%d\n", ntohs(hdr->icmp_id));
	printf("seq:\t%d\n", ntohs(hdr->icmp_seq));
}
#else
void print_icmphdrinfo(const struct icmp6_hdr * hdr)
{
	/* IPv6 */
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
		
		/* ICMPv6 Errors */
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
#endif

/* gets a pointer to the internet address of a sockaddr* */
void * getinaddr(const struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) /* IPv4 */
		return &(((struct sockaddr_in*)sa)->sin_addr);
		
	return &(((struct sockaddr_in6*)sa)->sin6_addr); /* IPv6 */
}

#if IPV4
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

/* Computes an IPv4 ICMP checksum 
 * will crash if you feed it REALLY large packets (above 65535 bytes)
 * that shouldn't be allowed */
uint16_t icmp4_checksum(struct icmp icmphdr, const uint8_t * payload,
	size_t payloadlen)
{
	char buf[IP_MAXPACKET /* 65535 */];
	char * p;
	size_t cksumlen = 0;
	
	p = &buf[0];
	
	/* Copy type to buf (1 byte) */
	memcpy(p, &icmphdr.icmp_type, sizeof(icmphdr.icmp_type));
	p += sizeof(icmphdr.icmp_type);
	cksumlen += sizeof(icmphdr.icmp_type);
	
	/* Copy code to buf (1 byte) */
	memcpy(p, &icmphdr.icmp_code, sizeof(icmphdr.icmp_code));
	p += sizeof(icmphdr.icmp_code);
	cksumlen += sizeof(icmphdr.icmp_code);
	
	/* Copy checksum to buf (2 bytes), but it's not known, so 0
	 * according to RFC 792 */
	*p = 0; p++;
	*p = 0; p++;
	cksumlen += 2;
	
	/* Copy identifier to buf (2 bytes) */
	memcpy(p, &icmphdr.icmp_id, sizeof(icmphdr.icmp_id));
	p += sizeof(icmphdr.icmp_id);
	cksumlen += sizeof(icmphdr.icmp_id);
	
	/* Copy seq to buf (2 bytes) */
	memcpy(p, &icmphdr.icmp_seq, sizeof(icmphdr.icmp_seq));
	p += sizeof(icmphdr.icmp_seq);
	cksumlen += sizeof(icmphdr.icmp_seq);
	
	/* Copy payload */
	memcpy(p, payload, payloadlen);
	p += payloadlen;
	cksumlen += payloadlen;
	
	return checksum((uint16_t *)buf, cksumlen);
}
#endif


int main(int argc, char **argv)
{
	if (argc < 2)
	{
		printf("Usage: %s <hostname>\n", argv[0]);
		return EXIT_FAILURE;
	}
	
	/* TODO: Set by program args */
	int ttl = 0; /* IPvX time to live */
	
	int sock, r;
	struct addrinfo hints, *target_info, *p;
	
	#if IPV4
	char ipbuffer[INET_ADDRSTRLEN];
	#else
	char ipbuffer[INET6_ADDRSTRLEN];
	#endif
	
	
	/* Give getaddrinfo some hints: A raw IPv6 socket for ICMP6! */
	memset(&hints, 0, sizeof(struct addrinfo));
	#if IPV4
	hints.ai_family = AF_INET;
	hints.ai_protocol = IPPROTO_ICMP;
	#else
	hints.ai_family = AF_INET6;
	hints.ai_protocol = IPPROTO_ICMPV6;
	#endif
	
	hints.ai_socktype = SOCK_RAW;
	
	
	r = getaddrinfo(argv[1], NULL, &hints, &target_info);
	
	if (r != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(r));
		exit(EXIT_FAILURE);
	}
	
	/* Loop through possibilities */
	for (p = target_info; p != NULL; p = target_info->ai_next)
	{	
		/* Print out IP */	
		inet_ntop(
			p->ai_family,
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
		#if IPV4
		if (setsockopt(sock, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) < 0)
		{
			perror("setsockopt failed while setting IP_TTL");
		}
		#else
		if (setsockopt(sock, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
				&ttl, sizeof(ttl)) < 0)
		{
			perror("setsockopt failed while setting IPV6_UNICAST HOPS");
		}
		#endif
	}
	
	/* TODO: Do tha random id stuff..? */
	short id = 12345;
	
	#if IPV4
	struct icmp hdr;
	#else
	struct icmp6_hdr hdr;
	#endif
	
	int pid = fork();
	if (pid < 0)
	{
		ferr("fork");
	}
	else if (pid == 0)
	{
		/* child */
		struct sockaddr * resp_addr;
		
		#if IPV4
		socklen_t resp_addrlen = sizeof(struct sockaddr_in);
		#else
		socklen_t resp_addrlen = sizeof(struct sockaddr_in6);
		#endif
		
		resp_addr = malloc(resp_addrlen);
		
		char packet[MAX_PACKET_LEN];
		memset(packet, 0, MAX_PACKET_LEN);
		
		char * buf = NULL;
		size_t buf_size;
		#if IPV4
		size_t ipv4offset;
		#endif
		
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
			
			
			#if IPV4
			/* recvfrom for IPv4 will also include the IPv4 address */
			/* So, the second nimble(4 bits) of an IPv4 address
			 * contains the length of the address in terms of 32 bit
			 * - or 4 byte - words. So this will extract it. */
			ipv4offset = (*((uint8_t *)packet) & 0x0f) * 4;
			#define ADDITIONAL_OFFSET ipv4offset
			#else
			/* The IPv6 stack will strip the IPv6 header off, leaving
			 * the data we want */
			#define ADDITIONAL_OFFSET 0
			#endif
			
			#define MSG_OFFSET (ADDITIONAL_OFFSET + sizeof(hdr))
			
			
			memcpy(&hdr, packet + ADDITIONAL_OFFSET, sizeof(hdr));
			
			#if IPV4
			if (hdr.icmp_type == ICMP_ECHO) /* echo request, 8 */
			{
				printf("Incoming message!\n");
			}
			else if (hdr.icmp_type == ICMP_ECHOREPLY) /* 0 */
			{
				if (ntohs(hdr.icmp_id) == id)
				{
					printf("Message acknowledged!\n");
				}
			}
			else
			{
				/* ??? */
			}
			#else
			
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
			#endif
			
			printf("Got packet from %s\n", ipbuffer);
			print_icmphdrinfo(&hdr);
			
			buf_size = r - MSG_OFFSET;
			
			buf = realloc(buf, buf_size);
			
			memcpy(buf, packet + MSG_OFFSET, buf_size);
			
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
			memset(&hdr, 0, sizeof(hdr));
			
			#if IPV4
			/* Craft ICMP header */
			hdr.icmp_type = ICMP_ECHO;
			hdr.icmp_code = 0;
			hdr.icmp_cksum = 0; /* Will NOT be computed by the IP stack */
			hdr.icmp_id = htons(id);
			hdr.icmp_seq = htons(seq);
			#else
			/* Craft ICMPv6 header */
			hdr.icmp6_type = ICMP6_ECHO_REQUEST;
			hdr.icmp6_code = 0;
			hdr.icmp6_cksum = 0; /* Will be calculated by the IP stack */
			hdr.icmp6_id = htons(id);
			hdr.icmp6_seq = htons(seq);
			#endif
			
			r = getline(&msg, &msg_alloc_len, stdin);
			
			if (r < 0)
				ferr("getline");
				
			msg_len = (size_t)r;
			
			#if IPV4
			hdr.icmp_cksum = icmp4_checksum(hdr, (uint8_t *)msg, msg_len);
			#endif
			
			print_icmphdrinfo(&hdr);
			
				
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
