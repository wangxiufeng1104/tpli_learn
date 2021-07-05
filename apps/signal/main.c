#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include "error_functions.h"
#define SIGNAL_ALARM
#ifdef SIGNAL_ALARM
pthread_t alarm_thread_t;
int Cnt = 0;
void signal_alarm_callback(int signo)
{
    //输出定时提示信息
    printf("   seconds: %d", ++Cnt);
    printf("\r");
    //重新启动定时器，实现1秒定时
    alarm(1);
}
void *signal_alarm_thread(void *argv)
{
    //安装信号
    if (signal(SIGALRM, signal_alarm_callback) == SIG_ERR)
    {
        errExit("SIGALARM");
    }
    //关闭标准输出的行缓冲模式
    setbuf(stdout, NULL);
    //启动定时器
    alarm(1);
    while (1)
    {
        //暂停，等待信号
        pause();
    }
}
#endif
int main()
{

#ifdef SIGNAL_ALARM
    if(pthread_create(&alarm_thread_t, NULL, signal_alarm_thread, NULL) != 0)
    {
        errExitEN(errno,"pthread_create");
    }
#endif
    while (1)
    {
        //暂停，等待信号
        pause();
    }
    
}
