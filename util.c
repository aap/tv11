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
	struct sockaddr_in serv_addr;

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if(sockfd < 0){
		perror("error: socket");
		return -1;
	}
	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	inet_pton(AF_INET, host, &serv_addr.sin_addr);
	serv_addr.sin_port = htons(port);

	if(connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
		perror("error: connect");
		return -1;
	}
	return sockfd;
}
