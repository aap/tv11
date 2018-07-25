#include "tv11.h"

#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

int
hasinput(int fd)
{
	fd_set fds;
	struct timeval timeout;

	timeout.tv_sec = 0;
	timeout.tv_usec = 0;
	FD_ZERO(&fds);
	FD_SET(fd, &fds);
	return select(fd+1, &fds, NULL, NULL, &timeout) > 0;
}

int
dial(char *host, int port)
{
	int sockfd;
	struct sockaddr_in server;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0){
		perror("error: socket");
		return -1;
	}
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	inet_pton(AF_INET, host, &server.sin_addr);
	server.sin_port = htons(port);
	if(connect(sockfd, (struct sockaddr*)&server, sizeof(server)) < 0){
		perror("error: connect");
		return -1;
	}
	return sockfd;
}

void
serve(int port, void (*handlecon)(int, void*), void *arg)
{
	int sockfd, confd;
	socklen_t len;
	struct sockaddr_in server, client;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0){
		perror("error: socket");
		return;
	}
	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = htons(port);
	if(bind(sockfd, (struct sockaddr*)&server, sizeof(server)) < 0){
		perror("error: bind");
		return;
	}
	listen(sockfd, 5);
	len = sizeof(client);
	while(confd = accept(sockfd, (struct sockaddr*)&client, &len),
	      confd >= 0)
		handlecon(confd, arg);
	perror("error: accept");
	return;
}
