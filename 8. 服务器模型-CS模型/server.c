// ��������� _GNU_SOURCE���ų��� fcntlͷ�ļ��ᱨ�� 
#define _GNU_SOURCE 1

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <string.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <fcntl.h>

int main(int argc, char* argv[]) {

	if(argc <= 2) {
		printf("Wrong number of parameters!\n");
		return 1;
	}

	char* ip = argv[1];
	int port = atoi(argv[2]);
	struct sockaddr_in address_server, address_client;
	memset(&address_server, 0, sizeof(address_server));
	address_server.sin_family = AF_INET;
	address_server.sin_port = htons(port);
	inet_pton(AF_INET, ip, &address_server.sin_addr);

	int sock_server = socket(PF_INET, SOCK_STREAM, 0);
	assert(sock_server >= 0);
	int ret = bind(sock_server, (struct sockaddr*)&address_server, sizeof(address_server));
	assert(ret != -1);
	ret = listen(sock_server, 5);
	assert(ret != -1);
	
	// readfds ����洢�ͻ���  testfds ����洢�䶯���ļ������� 
	fd_set readfds, testfds;

	FD_ZERO(&readfds);
	FD_SET(sock_server, &readfds);

	while(1) {
		
		testfds = readfds;
		// ��ȡ�䶯���ļ������� д���� 
		int result_select = select(FD_SETSIZE, &testfds, (fd_set *)0, (fd_set *)0, (struct timeval*)0);
		if(result_select < 1) {
			perror("server error\n");
			exit(1);
		}
		
		int count_fd; 
		for(count_fd = 0; count_fd < FD_SETSIZE; count_fd++) {
			
			if(FD_ISSET(count_fd, &testfds)) {
				
				// ���Ϊ ������fd ������µ����Ӳ����� readfds
				if(count_fd == sock_server) {
					int len_client = sizeof(address_client);
					int sock_client = accept(sock_server, (struct sockaddr*)&address_client, &len_client);
					FD_SET(sock_client, &readfds);
					printf("add a new client to readfds %d\n", sock_client);
				} else {
				
					// ���ؿͻ��˷��͵���Ϣ 
					int pipefd[2];
					int result_pipe = pipe(pipefd);
					// ����SPLICE_F_NONBLOCK �Ƕ�����ֹ�ͻ��˵��ߵĵȴ� 
					result_pipe = splice(count_fd, NULL, pipefd[1], NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
					// �ͻ��˵��ߺ�����ͻ��� 
					if(result_pipe == -1) {
						close(count_fd);
						FD_CLR(count_fd, &readfds);
					}
					
					result_pipe = splice(pipefd[0], NULL, count_fd, NULL, 32768, SPLICE_F_MORE | SPLICE_F_MOVE | SPLICE_F_NONBLOCK);
				}
			}
		}
	}
}
