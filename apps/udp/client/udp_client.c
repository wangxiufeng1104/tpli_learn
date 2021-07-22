#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <sys/time.h>
#define CMD_START 0
#define CMD_STOP 1
#define CMD_PAUSE 2
#define CMD_RESUME 3
#define IP_ADDR "127.0.0.1"
#define T_S 1000000

int server_fd;
#define LOG_WARN(fmt, ...) printf("warning@%s %d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) printf("info@%s %d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) printf("err@%s %d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#pragma pack(1)
typedef struct
{
    uint8_t cmd;
    uint64_t t_start;
    uint64_t t_end;
    uint64_t t_beg_offset;
    uint64_t t_end_offset;
    uint64_t t_cur_offset;
    uint8_t file_name[0];
} cmd_info_t;
#pragma pack()
uint64_t get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv,NULL);
    return ((tv.tv_sec * 1000000) + (tv.tv_usec)); 
}
char send_data_buf[1024];
//通过客户端连接哪个服务器
// ./client [127.0.0.1]  [9090]
//         环回ip，表示自己的ip地址？发送数据是我自己，接收方也是我自己
void cmd_handle(char *str, int len, cmd_info_t *cmd_info)
{
    int cmd_num = atoi(&str[0]);
    switch (cmd_num)
    {
    case CMD_START:
    case CMD_STOP:
    case CMD_PAUSE:
    case CMD_RESUME:
        cmd_info->cmd = cmd_num;
        break;
    default:
        break;
    }
    cmd_info->t_start = get_time_us();
    cmd_info->t_end = cmd_info->t_start + 10 * T_S;
    cmd_info->t_beg_offset = 0;
    cmd_info->t_end_offset = 2 * T_S;
    cmd_info->t_cur_offset = 1 * T_S;
}
int main(int agrc, char *argv[])
{
    cmd_info_t *cmd_info = (cmd_info_t *)send_data_buf;
    if (agrc != 3)
    {
        printf("Usage ./client [port] [file_name]]\n");
        return 1;
    }
    //创建一个socket，协议族，面向数据报
    int server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }

    struct sockaddr_in server_addr;                   //保存服务器端的ip地址和端口号
    server_addr.sin_family = AF_INET;                 //协议族
    server_addr.sin_addr.s_addr = inet_addr(IP_ADDR); //将ip地址进行强转，unint_32

    server_addr.sin_port = htons(atoi(argv[1]));      //将端口号强转为unint_16
    memcpy(cmd_info->file_name, argv[2], strlen(argv[2]));

    //如果客户端不绑定端口号，操作系统会在通信时自动分配一个端口号
    //一般不绑定端口号，由于客户端的机器装了哪些程序不确定
    //如果强行绑定端口号，就可能和客户端的其他程序冲突

    while (1)
    {
        char buf[1024] = {0};
        ssize_t read_size = read(0, buf, sizeof(buf) - 1);
        if (read_size < 0)
        { //客户端从标准输入读取数据出错，可以进行退出，因为一个服务器可以对应多个客户端
            perror("read");
            return 2;
        }

        if (read_size == 0)
        { //read返回值为0表示读到文件结束EOF
            printf("read done\n");
            return 0; //表示代码跑完，结果正确
        }
        buf[read_size] = '\0';
        cmd_handle(buf, read_size, cmd_info);
        //向服务器发数据
        sendto(server_fd, (void *)cmd_info, sizeof(cmd_info_t) + strlen(cmd_info->file_name), 0,
               (struct sockaddr *)&server_addr, sizeof(server_addr));

        // char buf_output[1024] = {0};
        // read_size = recvfrom(server_fd, buf_output, sizeof(buf_output) - 1, 0,
        //                      NULL, NULL); //不关心服务器的端口号和ip，因为客户端一定知道从哪个服务器端返回的数据
        // if (read_size < 0)
        // {
        //     perror("recvfrom");
        //     return 2;
        // }
        // buf_output[read_size] = '\0';
        // printf("server respon: %s\n", buf_output);
    }

    return 0;
}