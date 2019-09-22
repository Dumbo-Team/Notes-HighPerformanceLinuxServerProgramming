//
// Created by lsmg on 2019/9/20.
//

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <unistd.h>
#include <cstring>
#include <sys/epoll.h>
#include <pthread.h>
#include <fcntl.h>
#include <cerrno>
#include "lst_timer.h"
#define FD_LIMIT 65535
#define MAX_EVENT_NUMBER 1024
#define TIMESLOT 5

static int pipefd[2];
static sort_timer_lst timer_lst;
static int epollfd = 0;

int setnonblocking(int fd)
{
	int old_option = fcntl(fd, F_GETFL);
	fcntl(fd, F_SETFL, old_option | O_NONBLOCK);
	return old_option;
}

void addfd(int epollfd, int fd)
{
	epoll_event event{};
	event.events = EPOLLIN | EPOLLET;
	event.data.fd = fd;
	epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &event);
	setnonblocking(fd);
}

// �źż��Ĵ������� �����źŵ�pipefd[1]�� Ȼ��Ӵ�pipefd[0]������Ϣ�� ������������epoll������
void sig_handler(int sig)
{
	int save_errno = errno;
	int msg = sig;
	send(pipefd[1], (char*)&msg, 1, 0);
	errno = save_errno;
}

// ����Ҫ������źŵ��źż�
void addsig(int sig)
{
	struct sigaction sa = {};
	sa.sa_handler = sig_handler; // ָ���źż��Ĵ�����
	sa.sa_flags |= SA_RESTART;
	sigfillset(&sa.sa_mask);
	assert(sigaction(sig, &sa, nullptr) != -1);
}

void timer_handler()
{
	timer_lst.tick();
	alarm(TIMESLOT);
}

// ɾ���ǻ����socket�ϵ�ע��ʱ�䣬 ���ر�
void cb_func(client_data* user_data)
{
	epoll_ctl(epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
	assert(user_data);
	close(user_data->sockfd);
	printf("close fd %d\n", user_data->sockfd);
}

int main(int argc, char* argv[])
{

	if(argc <= 2)
	{
		printf("Wrong number of parameters\n");
		return 1;
	}

	const char* ip = argv[1];
	int port = atoi(argv[2]);

	struct sockaddr_in address = {};
	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	inet_pton(AF_INET, ip, &address.sin_addr);

	int listenfd = socket(PF_INET, SOCK_STREAM, 0);
	assert(listenfd >= 0);

	int ret = bind(listenfd, (struct sockaddr*)&address, sizeof(address));
	assert(ret != -1);

	ret = listen(listenfd, 5);
	assert(ret != -1);

	epoll_event events[MAX_EVENT_NUMBER];
	int epollfd = epoll_create(5);
	assert(epollfd != -1);
	addfd(epollfd, listenfd);

	ret = socketpair(PF_UNIX, SOCK_STREAM, 0, pipefd);
	assert(ret != -1);
	setnonblocking(pipefd[1]);
	addfd(epollfd, pipefd[0]);

	addsig(SIGALRM);
	addsig(SIGTERM);
	bool stop_server = false;

	client_data* users = new client_data[FD_LIMIT];
	bool timeout = false;
	alarm(TIMESLOT);

	while (!stop_server)
	{
		/* 
		 * ���ﻹ����ͨ��IO���ú������ó�ʱʱ��epoll_wait��ʱ�䳬ʱ��᷵��0 ����������Ӧ��Ϣ
		 * ��ȻΪ�˱���������Ϣ������Ӱ�� ��ʱʱ����Ҫ����Ϊ������ ��֤ÿ һ���趨ʱ�� ����һ�κ���
		 * 
		 * ����ͨ����epoll_wait�趨һ��startʱ�� ��epoll_wait���趨endʱ�� �µĵȴ�ʱ���ȥʱ���
		 * �����ȥ���ʱ��С��0 ������Ϊ �涨������ʱ����
		 * 
		 * */
		int number = epoll_wait(epollfd, events, MAX_EVENT_NUMBER, -1);
		if ((number < 0) && (errno != EINTR))
		{
			printf("epoll failuer\n");
			break;
		}

		for (int i = 0; i < number; ++i)
		{
			int sockfd = events[i].data.fd;
			if (sockfd == listenfd)
			{
				struct sockaddr_in client_address;
				socklen_t  client_addrlength = sizeof(client_address);
				int connfd = accept(listenfd, (struct sockaddr*)&client_address, &client_addrlength);
				addfd(epollfd, connfd);
				users[connfd].address = client_address;
				users[connfd].sockfd = connfd;
				
				// ������ʱ�������ûص������볬ʱʱ�䣬 �󶨶�ʱ�����û�����
				util_timer* timer = new util_timer;
				timer->user_data = &users[connfd];
				timer->cb_func = cb_func;
				time_t cur = time(nullptr);
				
				// �趨 3  * TIMESLOT ��ʱʱ��, �����ǵ�ʱ���ִ��, ������Ҫepoll_wait���˷���ֵ��,����ÿ����һ�εĶ�ʱ��sig_handler()������
				// ���ܽ����·��� if�ж����� timeoutΪtrue, ����ִ��β����timeout�ж��е� timer_handler�����ж� timer_lst�еĳ�ʱʱ�� 
				timer->expire = cur + 3  * TIMESLOT;
				users[connfd].timer = timer;
				timer_lst.add_timer(timer);
			}
			else if ((sockfd == pipefd[0]) && (events[i].events & EPOLLIN))
			{
				int sig;
				char signals[1024];
				ret = recv(pipefd[0], signals, sizeof(signals), 0);
				if (ret == -1)
				{
					continue;
				}
				else if (ret == 0)
				{
					continue;
				}
				else
				{
					for (int i = 0; i < ret; ++i)
					{
						switch (signals[i])
						{
							case SIGALRM:
								// ����ж�ʱ������Ҫ���� ������Ҫ�������������ȴ�����������
								timeout = true;
								break;
							case SIGTERM:
								stop_server = true;
						}
					}
				}
			}
			
			// ����ͻ��˷�������Ϣ
			else if (events[i].events & EPOLLIN)
			{
				memset(users[sockfd].buf, '\0', BUFFER_SIZE);
				ret = recv(sockfd, users[sockfd].buf, BUFFER_SIZE - 1, 0);
				printf("get %d bytes of client data %sfrom %d\n", ret, users[sockfd].buf, sockfd);
				util_timer* timer = users[sockfd].timer;
				if (ret < 0)
				{
					if (errno != EAGAIN)
					{
						cb_func(&users[sockfd]);
						if (timer)
						{
							timer_lst.del_timer(timer);
						}
					}
				}
				// �Է��ر�����
				else if (ret == 0)
				{
					cb_func(&users[sockfd]);
					if (timer)
					{
						timer_lst.del_timer(timer);
					}
				}
				else
				{
					if (timer)
					{
						time_t cur = time(nullptr);
						timer->expire = cur + 3 * TIMESLOT;
						printf("adjust timer once\n");
						timer_lst.adjust_timer(timer);
					}
				}
				if (timeout)
				{
					timer_handler();
					timeout = false;
				}
			}
		}
	}
	close(listenfd);
	close(pipefd[1]);
	close(pipefd[0]);
	delete[] users;
	return 0;
}
