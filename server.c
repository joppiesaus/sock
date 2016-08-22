/*
 * server.c
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
 */


#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <strings.h>
#include <string.h> // string.h and strings.h...?
#include <poll.h>
#include <time.h>

#define MAX_CONN 100
#define BACKLOG 5 // listen() backlog
#define BUFLEN 256 // size of recv buffer
#define TIMEOUT 30 * 1000 // 30 seconds poll() timeout

const char * RETURN_MESSAGE = "✓✓ seen";
const char * FULL_MESSAGE = "😩 I am sorry but the server is full";
const char * TIME_BROKE = "sorry the time broke";

void ferr(const char * msg)
{
	perror(msg);
	exit(EXIT_FAILURE);
}

const char* getcurrenttimestringthing()
{
	time_t ct = time(NULL);
	
	if (ct == ((time_t)-1))
		return TIME_BROKE;
	
	char* tstr = ctime(&ct);
	
	return (tstr == NULL) ? TIME_BROKE : tstr;
}

// C COMES WITH EXTENDED OUT OF THE BOX SUPPORT FOR STRINGS
char * concatstr(const char * a, const char * b)
{
	char * c = (char *)malloc(1 + strlen(a) + strlen(b));
	strcpy(c, a);
	strcat(c, b);
	return c;
}

int main(int argc, char **argv)
{
	int sock, portno, rv, i, j, n;
	struct sockaddr_in serv_addr, cli_addr;
	
	char buffer[BUFLEN];
	
	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s <port>\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	// TODO: IPv6
	sock = socket(AF_INET, SOCK_STREAM, 0); // init tcp socket
	
	if (sock < 0)
		ferr("Error opening socket");
		
	const socklen_t yes = 1;
	
	// make the socket reusable
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
	{
		perror("Error on listen");
		exit(EXIT_FAILURE);		
	}
	
	socklen_t clilen = sizeof(cli_addr);
	
	//struct pollfd * fdlist;
	//fdlist = (struct pollfd *)malloc(sizeof(struct pollfd) * MAX_CONN);
	
	// TODO: Make "infinite"?
	struct pollfd fdlist[MAX_CONN + 1];
	fdlist[0].fd = sock;
	fdlist[0].events = POLLIN;
	
	int sockcount = 1;
	
	
	while (1)
	{
		rv = poll(fdlist, (nfds_t)sockcount, TIMEOUT);
		
		if (rv == -1)
			ferr("poll() failed");
		else if (rv == 0)
		{
			//printf("timeout\n");
			continue;
		}
		
		if (fdlist[0].revents & POLLIN)
		{
			// New connection inbound
			memset(&cli_addr, 0, sizeof(cli_addr));
			int cfd = accept(sock, (struct sockaddr *)&cli_addr,
				&clilen);
				
			if (cfd < 0)
			{
				perror("Error accepting client");
				continue; //???
			}
			
			if (sockcount > MAX_CONN)
			{
				// Server is full, reject
				printf("Client rejected(server full)\n");
				
				// Send a kind message
				n = send(cfd, FULL_MESSAGE, strlen(FULL_MESSAGE), 0);
				
				if (n < 0)
					perror("Failed to write server is full response");
					
				close(cfd);
				
				continue; //???
			}
			
			// Add it to the list
			struct pollfd client;
			client.fd = cfd;
			client.events = POLLIN;
			
			fdlist[sockcount++] = client;
			
			printf("Accepted new client\n");
		}
		
		for (i = 1/*skip listening socket*/; i < sockcount; i++)
		{
			// POLLHUP not required
			if (fdlist[i].revents & POLLIN)
			{
				memset(buffer, 0, BUFLEN);
				
				n = recv(fdlist[i].fd, buffer, BUFLEN - 1, 0);
				
				if (n < 0)
				{
					perror("Error reading socket");
					continue;
				}
				else if (n == 0)
				{
					// EOF detected, remove
					close(fdlist[i].fd);
					
					// Swap
					fdlist[i--] = fdlist[--sockcount];
					
					printf("Client left\n");
					
					continue;
				}
				
				printf("Received message: %s", buffer);
				
				n = send(fdlist[i].fd, RETURN_MESSAGE,
					strlen(RETURN_MESSAGE), 0);
					
				if (n < 0)
					perror("Error writing socket");
				
				
				// send it to the rest of the peers
				for (j = 1; j < sockcount; j++)
				{
					// Do not resend to sender
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
	
	
	
	/*newsockfd = accept(sock, (struct sockaddr*)&cli_addr, &clilen);
	if (newsockfd < 0)
		ferr("Error on accepting socket");
		
	memset(buffer, 0, BUFLEN);
	
	n = read(newsockfd, buffer, BUFLEN - 1);
	if (n < 0)
		ferr("Error on reading socket");
	
	printf("Message received: %s\n", buffer);
	
	n = write(newsockfd, RETURN_MESSAGE, strlen(RETURN_MESSAGE));
	
	if (n < 0)
		ferr("Error on writing socket");
		
	*/
	
	//while (1)
	//{
		/*newsockfd = accept(sock, (struct sockaddr*)&cli_addr,
			&clilen);
		
		if (newsockfd < 0)
		{
			fprintf(stderr, "Error accepting socket\n");
			continue;
		}*/
		
		
		/*int pid = fork();
		if (pid < 0)
			ferr("Error on fork");
		else if (pid == 0)
		{
			// child
			close(sock);
			
			while (1)
			{
				memset(buffer, 0, BUFLEN);
				n = read(newsockfd, buffer, BUFLEN - 1);
				if (n < 0)
					ferr("Error reading socket");
				if (n == 0)
				{
					printf("Received EOF\n");
					break;
				}
				printf("Message received: %s\n", buffer);
				
				/ *char * msg = concatstr(
					RETURN_MESSAGE,
					getcurrenttimestringthing()
				);
				
				n = write(newsockfd, msg, strlen(msg));* /
				
				n = write(newsockfd, RETURN_MESSAGE,
					strlen(RETURN_MESSAGE));
				
				if (n < 0)
					ferr("Error on writing socket");
			}
			close(newsockfd);
			exit(EXIT_SUCCESS);
		}
		else
		{
			// parent
			close(newsockfd);
		}*/
	//}
	
	// take off socks
	//close(newsockfd);
	//close(sock);
	
	return 0;
}

