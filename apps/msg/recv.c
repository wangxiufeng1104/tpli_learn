#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>
#include <stdint.h>
#include "que.h"

#define Q 0x4000

uint64_t localtime_ms(){
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return tv.tv_sec*1000 + tv.tv_usec/1000;
}

void *pthread_handler(void* arg){
    int timeout = *(int*)arg+1,ret;
    free(arg);
    timeout *= 1000;
    unsigned char data[1000];
    uint64_t old,now;
    for(;;){
        memset(data,0,sizeof(data));
        old = localtime_ms();
        ret = read_que_timedwait(Q,data,sizeof(data),timeout);
        now = localtime_ms();
        printf("%lu timeout %d, use time%lu, ret=%d, errno=%d, data%s\n",pthread_self(),timeout,now-old,ret,errno,data);
    }

    pthread_exit(NULL);
}

int main(){
    int i;
    pthread_t tid[10];

    for(i=0;i<sizeof(tid)/sizeof(pthread_t);i++){
        int *j = malloc(sizeof(int));
        *j = i;
        pthread_create(&tid[i],NULL,pthread_handler,j);
    }

    pause();
    return 0;
}
