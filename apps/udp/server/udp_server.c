#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> /* See NOTES */
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#define LOG_WARN(fmt, ...) printf("warning@%s %d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOG_INFO(fmt, ...) printf("info@%s %d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LOG_ERROR(fmt, ...) printf("err@%s %d: " fmt, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define IP_ADDR "127.0.0.1"
#define CMD_START 0
#define CMD_STOP 1
#define CMD_PAUSE 2
#define CMD_RESUME 3
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
} cmd_msg_t;
#pragma pack()
pthread_t g_replay_can_thread;
typedef struct
{
    struct sockaddr_in addr;
} client_info;

void *replay_can(void *argv)
{
    int count = 0;
    while (1)
    {
        printf("\rcount:%d", count++);
        fflush(stdout);
        sleep(1);
    }
}
void cmd_start_handle(cmd_msg_t *cmd_msg)
{
    int res = 0;
    LOG_INFO("file_name:%s\n", cmd_msg->file_name);
    pthread_create(&g_replay_can_thread, NULL, replay_can, NULL);
    if (res != 0)
    {
        LOG_ERROR("pthread_create failed\n");
    }
}

void cmd_stop_handle(cmd_msg_t *cmd_msg)
{
    int res = 0;
    void *ret;
    res = pthread_cancel(g_replay_can_thread);
    if (res != 0)
    {
        LOG_ERROR("pthread_cancle failed\n");
    }
    res = pthread_join(g_replay_can_thread, &ret);
    if (res != 0)
    {
        LOG_ERROR("pthread_join\n");
    }
    if (ret == PTHREAD_CANCELED)
    {
        g_replay_can_thread = 0;
        LOG_INFO("thread was cancle\n");
    }
    else
        LOG_ERROR("thread was not cancle\n");
}
void cmd_pause_handle(cmd_msg_t *cmd_msg)
{
}
void cmd_resume_handle(cmd_msg_t *cmd_msg)
{
}
void cmd_handle(cmd_msg_t *cmd_msg)
{
    switch (cmd_msg->cmd)
    {
    case CMD_START:
        LOG_INFO("CMD_START\n");
        cmd_start_handle(cmd_msg);
        break;
    case CMD_STOP:
        LOG_INFO("CMD_STOP\n");
        cmd_stop_handle(cmd_msg);
        break;
    case CMD_PAUSE:
        LOG_INFO("CMD_PAUSE\n");
        break;
    case CMD_RESUME:
        LOG_INFO("CMD_RESUME\n");
        break;
    default:
        LOG_WARN("illegal cmd\n");
        break;
    }
}
char g_cmd_buf[1024];
client_info g_client_info;
int main(int argc, char **argv)
{
    cmd_msg_t *cmd_msg;
    /*以下代码相当于服务器启动时的准备工作*/
    if (argc != 2)
    { //进程 ip地址, 端口号
        printf("Usage ./server [port]\n");
        return 1; //退出码,表示程序跑完,结果出错
    }
    int fd = socket(AF_INET, SOCK_DGRAM, 0); //1.哪个协议族，面向数据报
    if (fd < 0)
    { //文件描述符被占用,出错;一个进程的文件描述符最多为ulimit -a
        perror("socket");
        return 2;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;                 //协议族
    addr.sin_addr.s_addr = inet_addr(IP_ADDR); //将10进制IP地址转换成unint_32ip
    addr.sin_port = htons(atoi(argv[1]));      //将端口号转成整数，在将其转成字节序
    
    //将文件描述符进行绑定,其实是将端口号绑定
    int ret = bind(fd, (struct sockaddr *)&addr, sizeof(addr)); //指针强转
    if (ret < 0)
    { //如果端口号被其他进程占用，会失败
        perror("bind");
        return 3;
    }

    while (1)
    {                                 //服务器一旦启动，不会进行退出，会一直运行下去，为了反复的从客户端接受数据，处理数据，向客户端返回数据
        struct sockaddr_in peer;      //结构体，里面存着对应端的ip和端口号（这里指源ip和源端口号）
        socklen_t len = sizeof(peer); //相当于缓冲区的长度,输入输出型的参数（理论上没有变化）

        ssize_t read_size = recvfrom(fd, g_cmd_buf, sizeof(g_cmd_buf), 0,
                                     (struct sockaddr *)&peer, &len); //缓冲区的大小预留一个字节，用来结束字符串\0，sockaddr* src_addr是输出型参数，一旦从客户端返回，就可以拿到客户端的ip地址和端口号（源ip，源端口号）

        if (read_size < 0)
        { //说明执行失败，当服务器执行失败后，尝试下一次的拿数据
            perror("recvfrom");
            continue;
        }
        g_cmd_buf[read_size] = '\0'; //返回值表示读到多少字节
        LOG_INFO("read_size:%ld\n", read_size);
        cmd_msg = (cmd_msg_t *)g_cmd_buf;
        cmd_handle(cmd_msg);
        //a -> ascii
        printf("client %s: %d \n",
               inet_ntoa(peer.sin_addr), ntohs(peer.sin_port));
        g_client_info.addr.sin_addr = peer.sin_addr;
        g_client_info.addr.sin_port = peer.sin_port;
        //将收到的数据发给哪个socket，发的数据在哪里，要发给哪个端口号,这里指刚才接收到的源ip和源端口号
        //sendto(fd, buf, strlen(buf), 0, (struct sockaddr *)&peer, len); //将收到的数据返回给客户端
    }
    close(fd); //一般执行不到
    return 0;
}