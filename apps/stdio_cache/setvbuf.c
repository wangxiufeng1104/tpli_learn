#include <stdio.h>
#include <unistd.h>
#include "error_functions.h"

#define BUF_SIZE 1024
char buf[BUF_SIZE];
int count  = 0;
int main()
{
    if(setvbuf(stdout, buf, _IONBF, BUF_SIZE) != 0)
        errExit("setvbuf");
    
    printf("123213");
    while(1)
    {
        printf("123");
        sleep(1);
        if(((count ++) % 10) == 0)
            printf("\n");
    }
}