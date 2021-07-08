#include "common.h"
/***************
 * 删除文件操作
 * ************/
int unlink_file(char *file_path)
{
    int ret = 0;
    if((ret = access(file_path, F_OK)) != 0)
        errExit("file not available:%d", ret);
    if((ret = unlink(file_path) == 0))
        printf("file unlink success\n");
    return ret;
}