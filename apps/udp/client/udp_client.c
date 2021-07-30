#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <fcntl.h>
#include <sys/time.h>
#include <signal.h>
#include <errno.h>
#include <semaphore.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#define CMD_START 0
#define CMD_STOP 1
#define CMD_PAUSE 2
#define CMD_RESUME 3
#define IP_ADDR "127.0.0.1"
#define T_S 1000000
typedef enum
{
    STATE_IDEL,
    STATE_START,
    STATE_STOP,
    STATE_PAUSE,
} state_enum;
int server_fd;
state_enum server_status;
void get_sys_time(void)
{
    struct timeval tv;
    struct tm *tm_ptr;
    gettimeofday(&tv, NULL);
    tm_ptr = localtime(&tv.tv_sec);
    printf("[%d-%02d-%02d %02d:%02d:%02d.%ld] ", 1900 + tm_ptr->tm_year, 1 + tm_ptr->tm_mon,
           tm_ptr->tm_mday, tm_ptr->tm_hour, tm_ptr->tm_min, tm_ptr->tm_sec, tv.tv_usec / 1000);
}
#define LOG_DEBUG(format, ...)                                                       \
    do                                                                               \
    {                                                                                \
        get_sys_time();                                                              \
        printf("[%s] [%6d] [DEBUG] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#define LOG_INFO(format, ...)                                                        \
    do                                                                               \
    {                                                                                \
        get_sys_time();                                                              \
        printf("[%s] [%6d] [INFO ] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#define LOG_WARN(format, ...)                                                        \
    do                                                                               \
    {                                                                                \
        get_sys_time();                                                              \
        printf("[%s] [%6d] [WARN ] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#define LOG_ERROR(format, ...)                                                       \
    do                                                                               \
    {                                                                                \
        get_sys_time();                                                              \
        printf("[%s] [%6d] [ERROR] " format, __FUNCTION__, __LINE__, ##__VA_ARGS__); \
    } while (0)
#pragma pack(1)
typedef struct
{
    uint8_t cmd;
    uint64_t t_start;
    uint64_t t_resume;
    uint64_t t_beg_offset;
    uint64_t t_end_offset;
    uint64_t t_cur_offset;
    uint8_t file_name[0];
} cmd_info_t;
#pragma pack()

uint64_t get_time_ms()
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return tv.tv_sec * 1000 + tv.tv_usec / 1000;
}
uint64_t get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((tv.tv_sec * 1000000) + (tv.tv_usec));
}
char send_data_buf[1024];
//通过客户端连接哪个服务器
// ./client [127.0.0.1]  [9090]
//         环回ip，表示自己的ip地址？发送数据是我自己，接收方也是我自己
void cmd_handle(char *str, int len, cmd_info_t *cmd_info)
{
    int cmd_num = atoi(&str[0]);
    cmd_info->cmd = cmd_num;
    switch (cmd_num)
    {
    case CMD_START:
        //播放 1 到 2 秒之间的数据
        cmd_info->t_start = get_time_ms() + 1000;
        cmd_info->t_resume = 0;
        cmd_info->t_beg_offset = 1000; //1000 ms
        cmd_info->t_end_offset = 50000;
        cmd_info->t_cur_offset = 0;
        server_status = STATE_START;
        break;
    case CMD_STOP:
        server_status = STATE_STOP;
        break;
    case CMD_PAUSE:
        server_status = STATE_PAUSE;
        break;
    case CMD_RESUME:
        server_status = STATE_START;
        break;
    default:
        break;
    }
}
int main(int agrc, char *argv[])
{
    struct timeval tv;
    cmd_info_t *cmd_info = (cmd_info_t *)send_data_buf;
    if (agrc != 3)
    {
        printf("Usage ./client [port] [file_name]]\n");
        return 1;
    }
    
    tv.tv_sec = 1;  //  注意防core
	tv.tv_usec =  0;
    //创建一个socket，协议族，面向数据报
    int server_fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_fd < 0)
    {
        perror("socket");
        return 1;
    }
    setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    struct sockaddr_in server_addr;                   //保存服务器端的ip地址和端口号
    server_addr.sin_family = AF_INET;                 //协议族
    server_addr.sin_addr.s_addr = inet_addr(IP_ADDR); //将ip地址进行强转，unint_32

    server_addr.sin_port = htons(atoi(argv[1])); //将端口号强转为unint_16
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
        LOG_INFO("send cmd\n");
        sendto(server_fd, (void *)cmd_info, sizeof(cmd_info_t) + strlen(cmd_info->file_name), 0,
               (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (server_status == STATE_PAUSE)
        {
            char buf_output[1024] = {0};
            cmd_info_t *cmd = (cmd_info_t *)buf_output;
            read_size = recvfrom(server_fd, buf_output, sizeof(buf_output) - 1, 0,
                                 NULL, NULL); //不关心服务器的端口号和ip，因为客户端一定知道从哪个服务器端返回的数据
            if (read_size < 0)
            {
                perror("recvfrom");
            }
            buf_output[read_size] = '\0';

            LOG_INFO("cmd:%d\n",cmd->cmd);
            LOG_INFO("start:%ld\n",cmd->t_start);
            LOG_INFO("resume:%ld\n",cmd->t_resume);
            LOG_INFO("beg_offset:%ld\n",cmd->t_beg_offset);
            LOG_INFO("cur_offset:%ld\n",cmd->t_cur_offset);
            LOG_INFO("end_offset:%ld\n",cmd->t_end_offset);
            LOG_INFO("file_name:%s\n",cmd->file_name);
        }
    }

    return 0;
}