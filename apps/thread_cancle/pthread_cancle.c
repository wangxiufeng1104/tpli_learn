#include <pthread.h>
#include "tlpi_hdr.h"


static void *thrFunc(void *arg)
{
    int count = 0;
    while(1)
    {
        printf("count:%d\n", count++);
        sleep(1);
    }
}

int main(int argc, char **argv)
{
    pthread_t thr;
    int res = 0;
    void *ret;
    while(1)
    {
        res = pthread_create(&thr, NULL, thrFunc, NULL);
        if(res != 0)
        {
            errExitEN(res, "pthread_create");
        }
        sleep(3);

        res = pthread_cancel(thr);
        if(res != 0)
        {
            errExitEN(res, "pthread_cancle");
        }
        res = pthread_join(thr, &ret);
        if(res != 0)
        {
            errExitEN(res, "pthread_join");
        }
        if(ret == PTHREAD_CANCELED)
        {
            printf("thread was cancle\n");
        }
        else
        {
            printf("thread was not cancle\n");
        }
    }
    exit(EXIT_SUCCESS);
}