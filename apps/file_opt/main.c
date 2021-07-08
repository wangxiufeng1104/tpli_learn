#include "common.h"

int main(int argc, char **argv)
{
    int ret = 0;
    ret = unlink_file("./123.txt");
    printf("ret:%d\n", ret);
    return 0;
}