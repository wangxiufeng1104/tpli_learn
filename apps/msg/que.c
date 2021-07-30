#include "que.h"
#include <string.h>		//for memcpy
#include <stdlib.h>		//for malloc free
#include <sys/time.h>	//for struct timeval gettimeofday
#include <signal.h>		//for signal sigset_t sigfillset pthread_sigmask SIG*
#include <sys/msg.h>	//for msgget msgsnd msgrcv
#include <sys/select.h> //for select fd_set FD_SET FD_ZERO
#include <unistd.h>		//for pipe write read close
#include <pthread.h>	//for pthread*
#include <errno.h>		//for errno E*
#include "rbtree.h"		//for rbtree*

#define MSGMAXLENGTH 2048
#define QUE_MSGTYPE 30l
#define QUE_LEVEL 0

static int event_alive = 0;
static pthread_t event_tid;
static pthread_mutex_t event_mutex = PTHREAD_MUTEX_INITIALIZER;
static int event_pipe_fd[2];

static rbtree_t event_time_rbtree;

typedef struct event_s event_t;
struct event_s
{
	pthread_t tid;
	rbtree_node_t event_time_rbtree_node;
	int event_status; //0 default, 1 timeout
};

int is_thread_alive(pthread_t tid)
{
	int kill_rc = pthread_kill(tid, 0); //0 success, ESRCH 线程不存在, EINVAL 信号不合法
	int ret = 1;						//alive
	if (kill_rc == ESRCH)
	{
		ret = 0;
	}
	return ret;
}

static void *event_timer_loop(void *arg);

static int event_timer_create()
{
	pthread_mutex_lock(&event_mutex);
	if (event_alive == 1)
	{
		pthread_mutex_unlock(&event_mutex);
		return 0;
	}

	pipe(event_pipe_fd);
	rbtree_init(&event_time_rbtree);
	pthread_create(&event_tid, NULL, event_timer_loop, NULL);
	event_alive = 1;

	pthread_mutex_unlock(&event_mutex);
	return 0;
}
/*
static int event_timer_destroy(){
	write(event_pipe_fd[1],"1",1);

	pthread_join(event_tid,NULL);

	close(event_pipe_fd[0]);
	close(event_pipe_fd[1]);

	rbtree_destory(&event_time_rbtree);
	event_alive = 0;
	return 0;
}
*/
static uint64_t localtime_ms()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}

static void *event_timer_loop(void *arg)
{
	rbtree_node_t *node;
	event_t *ev;

	uint64_t now, ev_time;
	int n;
	unsigned char tmp[1024];

	sigset_t sigset;
	sigfillset(&sigset);
	pthread_sigmask(SIG_SETMASK, &sigset, NULL);

	fd_set set, rset;
	FD_ZERO(&set);
	FD_SET(event_pipe_fd[0], &set);

	struct timeval tv, *timeout;

	for (;;)
	{
		pthread_mutex_lock(&event_mutex);

		node = rbtree_min(&event_time_rbtree, event_time_rbtree.root);
		if (node != NULL)
		{
			ev = rbtree_data(node, event_t, event_time_rbtree_node);
			ev_time = ev->event_time_rbtree_node.key;
		}

		now = localtime_ms();

		if (node != NULL && now >= ev_time)
		{
			ev->event_status = 1; //timeout
			rbtree_delete(&event_time_rbtree, &ev->event_time_rbtree_node);
			pthread_kill(ev->tid, SIGUSR1);
			pthread_mutex_unlock(&event_mutex);
			continue;
		}

		if (node == NULL)
		{
			timeout = NULL;
		}
		else
		{
			now = ev_time - now;
			timeout = &tv;
			timeout->tv_sec = now / 1000;
			timeout->tv_usec = (now % 1000) * 1000;
		}

		pthread_mutex_unlock(&event_mutex);
		rset = set;
		n = select(event_pipe_fd[0] + 1, &rset, NULL, NULL, timeout);
		if (n == 0)
		{
			//timeout
		}
		else if (n < 0)
		{
			//errno
		}
		else
		{
			n = read(event_pipe_fd[0], tmp, sizeof(tmp));
			for (; n > 0 && tmp[n - 1] == '0'; n--)
			{
			}
			if (n > 0)
			{
				break;
			}
		}
		//无论是有新节点插入，还是超时，由下一次loop处理
	}

	pthread_exit(NULL);
}

int add_timer(event_t *ev, uint64_t timeout)
{ //单位ms
	ev->tid = pthread_self();
	ev->event_time_rbtree_node.key = localtime_ms() + timeout;
	ev->event_status = 0;

	pthread_mutex_lock(&event_mutex);
	rbtree_insert(&event_time_rbtree, &ev->event_time_rbtree_node);
	pthread_mutex_unlock(&event_mutex);

	write(event_pipe_fd[1], "0", 1);
	return 0;
}

int delete_timer(event_t *ev)
{
	pthread_mutex_lock(&event_mutex);
	rbtree_delete(&event_time_rbtree, &ev->event_time_rbtree_node);
	pthread_mutex_unlock(&event_mutex);
	return 0;
}

void event_sighandler(int signo)
{
}

typedef struct
{
	long msgtype;
	unsigned char buffer[0];
} MSGBUF;

int send_que_timedwait(key_t qid, unsigned char *pdata, size_t len, int timeout)
{
	int msgqid, retu;
	event_t ev = {0};
	unsigned char *buf = NULL;
	MSGBUF *que_msgbuf;

	//https://man7.org/linux/man-pages/man2/msgget.2.html
	msgqid = msgget(qid, 0666);
	if (msgqid == -1)
	{
		if (errno == ENOENT)
		{
			msgqid = msgget(qid, IPC_CREAT | 0666);
		}
		if (msgqid == -1)
			return -1;
	}

	buf = malloc(sizeof(MSGBUF) + len);
	if (buf == NULL)
		return -1;

	que_msgbuf = (MSGBUF *)buf;
	que_msgbuf->msgtype = QUE_MSGTYPE;
	memcpy(que_msgbuf->buffer, pdata, len);

	// int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg)
	// msgflag
	// 0：当消息队列满时，msgsnd将会阻塞，直到消息能写进消息队列
	// IPC_NOWAIT：当消息队列已满的时候，msgsnd函数不等待立即返回
	// IPC_NOERROR：若发送的消息大于size字节，则把该消息截断，截断部分将被丢弃，且不通知发送进程。
	// 返回值
	// 成功：0
	// 出错：-1，错误原因存于error中
	// EAGAIN：参数msgflg设为IPC_NOWAIT，而消息队列已满
	// EIDRM：标识符为msqid的消息队列已被删除
	// EACCESS：无权限写入消息队列
	// EFAULT：参数msgp指向无效的内存地址
	// EINTR：队列已满而处于等待情况下被信号中断
	// EINVAL：无效的参数msqid、msgsz或参数消息类型type小于0

	if (timeout == -1)
	{
		retu = msgsnd(msgqid, buf, len, 0);
	}
	else if (timeout == 0)
	{
		retu = msgsnd(msgqid, buf, len, IPC_NOWAIT);
	}
	else
	{
		event_timer_create();

		signal(SIGUSR1, event_sighandler);
		add_timer(&ev, timeout);
		retu = msgsnd(msgqid, buf, len, 0);
		delete_timer(&ev);
	}

	free(buf);
	if (retu == 0)
	{
		return 0;
	}
	else if (ev.event_status == 1)
	{
		errno = ETIMEDOUT;
	}

	return -1;
}

int read_que_timedwait(key_t qid, unsigned char *pdata, size_t len, int timeout)
{
	int msgqid, retu;
	event_t ev = {0};
	unsigned char *buf = NULL;
	MSGBUF *que_msgbuf = NULL;

	msgqid = msgget(qid, 0666); //https://man7.org/linux/man-pages/man2/msgget.2.html
	if (msgqid == -1)
	{
		if (errno == ENOENT)
		{
			msgqid = msgget(qid, IPC_CREAT | 0666);
		}
		if (msgqid == -1)
			return -1;
	}

	buf = malloc(sizeof(MSGBUF) + len);
	if (buf == NULL)
	{
		return -1;
	}

	// ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp,int msgflg);
	// msgtyp
	// 0：接收第一个消息
	// >0：接收类型等于msgtyp的第一个消息
	// <0：接收类型等于或者小于msgtyp绝对值的第一个消息
	// msgflg
	// 0: 阻塞式接收消息，没有该类型的消息msgrcv函数一直阻塞等待
	// IPC_NOWAIT：如果没有返回条件的消息调用立即返回，此时错误码为ENOMSG
	// IPC_EXCEPT：与msgtype配合使用返回队列中第一个类型不为msgtype的消息
	// IPC_NOERROR：如果队列中满足条件的消息内容大于所请求的size字节，则把该消息截断，截断部分将被丢弃
	// 函数返回值
	// 成功：实际读取到的消息数据长度
	// 出错：-1，错误原因存于error中
	// E2BIG：消息数据长度大于msgsz而msgflag没有设置IPC_NOERROR
	// EIDRM：标识符为msqid的消息队列已被删除
	// EACCESS：无权限读取该消息队列
	// EFAULT：参数msgp指向无效的内存地址
	// ENOMSG：参数msgflg设为IPC_NOWAIT，而消息队列中无消息可读
	// EINTR：等待读取队列内的消息情况下被信号中断

	if (timeout == -1)
	{
		retu = msgrcv(msgqid, buf, len, 0, 0);
	}
	else if (timeout == 0)
	{
		retu = msgrcv(msgqid, buf, len, 0, IPC_NOWAIT);
	}
	else
	{
		event_timer_create();
		signal(SIGUSR1, event_sighandler);
		add_timer(&ev, timeout);
		retu = msgrcv(msgqid, buf, len, 0, 0);
		delete_timer(&ev);
	}

	que_msgbuf = (MSGBUF *)buf;

	if (retu > 0)
	{
		memcpy(pdata, que_msgbuf->buffer, retu);
	}

	free(buf);
	if (retu >= 0)
	{
		return retu;
	}
	else if (ev.event_status == 1)
	{
		errno = ETIMEDOUT;
	}

	return -1;
}
