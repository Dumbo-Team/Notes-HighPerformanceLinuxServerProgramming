#ifndef LST_TIMER
#define LST_TIMER

#include <ctime>
#include <cstdio>
#include <netinet/in.h>

#define BUFFER_SIZE 64

class util_timer;

// �û����ݽṹ socket��ַ�� �ļ��������� ������ �� ��ʱ��
struct client_data
{
	sockaddr_in address;
	int sockfd;
	char buf[BUFFER_SIZE];
	util_timer* timer;
};

class util_timer
{
public:
	util_timer() : prev(nullptr), next(nullptr) {}

public:
	time_t expire{}; // ����ʱʱ��
	void (*cb_func) (client_data*){}; // ����ص�����
	client_data* user_data{};
	util_timer* prev; // ָ��ǰһ����ʱ��
	util_timer* next; // ָ���һ����ʱ��
};

// ��ʱ��list ����˫������ ����ͷβ�ڵ�
class sort_timer_lst
{
public:
	sort_timer_lst() : head(nullptr), tail(nullptr) {}

	~sort_timer_lst()
	{
		util_timer* tmp = head;
		while (tmp)
		{
			head = tmp->next;
			delete tmp;
			tmp = head;
		}
	}

	void add_timer(util_timer* timer)
	{
		if (!timer)
		{
			return;
		}
		if (!head)
		{
			head = tail = timer;
		}
		// ����ʱ�������������еĵ���ʱ������뵽ͷ��
		if (timer->expire < head->expire)
		{
			timer->next = head;
			head->prev = timer;
			head = timer;
			return;
		}
		// ����ʱ����Ҫ���뵽����֮��
		add_timer(timer, head);
	}

	// ���ö�ʱ��ʱ ������ʱ����λ��
	void adjust_timer(util_timer* timer)
	{
		if (!timer)
		{
			return;
		}
		util_timer* tmp = timer->next;
		if (!tmp || (timer->expire < tmp->expire))
		{
			return;
		}

		// Ŀ�궨ʱ����ͷ�ڵ㣬 ���ö�ʱ��������ȡ�������²���
		if (timer == head)
		{
			head = head->next;
			head->prev = nullptr;
			timer->next = nullptr;
			add_timer(timer, head);
		}
		// ���Ŀ�궨ʱ������ͷ�ڵ㣬 �����������λ��֮��Ľڵ�
		else
		{
			timer->prev->next = timer->next;
			timer->next->prev = timer->prev;
			add_timer(timer, timer->next); // �ڶ����������������λ��֮��Ľڵ㣬 ����ʱ������
		}
	}

	void del_timer(util_timer* timer)
	{
		if (!timer)
		{
			return;
		}
		// ��ǰֻ����һ����ʱ���� ��ΪĿ�궨ʱ��
		if ((timer == head) && (timer == tail))
		{
			delete timer;
			head = nullptr;
			tail = nullptr;
			return;
		}
		// ��ǰ��ʱ����ֻ��һ���ڵ㣬 ��Ŀ�궨ʱ��Ϊͷ�ڵ㣬 �����ͷ�ڵ��ɾ��ԭͷ�ڵ�
		if (timer == head)
		{
			head = head->next;
			head->prev = NULL;
			delete timer;
			return;
		}
		// ������������ʱ���� �������β�ڵ�ָ��ǰһ���ڵ�
		if (timer == tail)
		{
			tail = tail->prev;
			tail->next = nullptr;
			delete timer;
			return;
		}
		timer->prev->next = timer->next;
		timer->next->prev = timer->prev;
		delete timer;
	}

	void tick()
	{
		if (!head)
		{
			return;
		}
		printf("timer tick\n");
		time_t cur = time(nullptr);
		util_timer* tmp = head;
		while (tmp)
		{
			if (cur < tmp->expire)
			{
				break;
			}
			// ���ûص������� ִ�ж�ʱ����
			tmp->cb_func(tmp->user_data);
			head = tmp->next;
			if (head)
			{
				head->prev = nullptr;
			}
			delete tmp;
			tmp = head;
		}
	}
private:
	void add_timer(util_timer* timer, util_timer* lst_head)
	{
		util_timer* prev = lst_head;
		util_timer* tmp = prev->next;
		while (tmp)
		{
			if (timer->expire < tmp->expire)
			{
				prev->next = timer;
				timer->next = tmp;
				tmp->prev = timer;
				timer->prev = prev;
				break;
			}
			prev = tmp;
			tmp = tmp->next;
		}
		if (!tmp)
		{
			prev->next = timer;
			timer->prev = prev;
			timer->next = nullptr;
			tail = timer;
		}
	}

private:
	util_timer* head;
	util_timer* tail;
};

#endif
