#include <stdio.h>
#include <unistd.h>
char buf[128];
int main(int argc, char **argv)
{
    FILE *fp;
    char c;
    fp = fopen("./test", "wb");
    fseek(fp, 127, SEEK_END);
    fwrite("1", 1 , 1, fp);
    fflush(fp);
    printf("创建空洞文件并写入\n");
    gets(&c);
    printf("开始移动文件指针到文件头\n");
    for(int i = 0; i < sizeof(buf); i++)
    {
        buf[i] = i;
    }
    fseek(fp, 0, SEEK_SET);
    fwrite(buf, 1 , sizeof(buf), fp);
    fflush(fp);
    fclose(fp);
    return 0;
}