/*****************************************************************************
File name: can_replay.c
Description: 用户通过上位机与网关通信，控制can_replay回放can数据给vci8
┌──────────────┐           ┌──────────────┐          ┌────────────┐
│              │ control   │              │ sysbus   │            │
│   gate_way   ├──────────►│  can_replay  ├─────────►│    vci8    │
│              │           │              │          │            │
└──────────────┘           └──────────────┘          └────────────┘
Author: Arno
Version: 1.0
Date: 20210723
History: 
20210723 Arno 初始版本
*****************************************************************************/
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
#include <libsysbus.h>
/**********************************************
 *                    Macro
 *********************************************/
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

#define IP_ADDR "127.0.0.1"
#define CMD_START 0
#define CMD_STOP 1
#define CMD_PAUSE 2
#define CMD_RESUME 3

#define RES_ERROR -1
#define RES_OK 0

#define T_S 1000000

#define MAX_MSG_BYTES 500

#define INDEX_TIME 0
#define INDEX_CHANNEL 1
#define INDEX_ID 2
#define INDEX_DIR 3
#define INDEX_D_FLAG 4
#define INDEX_DLC 5
#define INDEX_DATA 6
#define MAX_CHANNEL_NUM 1

#define NEED_REFRESH_BSAE_TIME 0
#define NO_NEED_REFRESH_BSAE_TIME  1
/**********************************************
 *                    variable
 *********************************************/
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
} cmd_msg_t;
#pragma pack()
typedef struct
{
    long type;
    uint64_t timestamp;
    uint64_t channel;
    uint64_t id;
    uint64_t dlc;
    uint8_t data[0];
} can_item_t;

typedef struct
{
    struct sockaddr_in addr;
} client_info_t;
typedef enum
{
    STATE_IDEL,
    STATE_START,
    STATE_PAUSE,
} state_enum;
typedef struct
{
    int sock_fd;
    state_enum server_state;
    cmd_msg_t cmd_msg;
    client_info_t client_info;
    sem_t pause_sem;
} server_info_t;

typedef struct
{
    pthread_t vci_thread;
    uint64_t channel;
    int channel_count;
    long msg_type;
    uint64_t cpu_time;
    FILE *fp;
    uint8_t refresh_base_time;
} vci_item_t;
char g_cmd_buf[1024];
char g_deal_buf[512];
char g_data_buf[512];
server_info_t g_server_info;
pthread_t g_replay_can_thread;
int g_msqid;
vci_item_t g_vci_item[MAX_CHANNEL_NUM];
int line_count = 0;
/**********************************************
 *                    function
 *********************************************/
/*************************************************
Function:     str2time
Description:  将can帧里的时间戳转换为微秒
Return: can帧的时间
**************************************************/
uint64_t str2time(char *s, can_item_t *item)
{
    char *p = NULL;
    double time_f;
    uint64_t time = 0;
    time_f = atof(s);
    time = (uint64_t)(time_f * 1000000);
    return time;
}
uint64_t get_time_us(void)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return ((tv.tv_sec * 1000000) + (tv.tv_usec));
}
void printf_can_item(can_item_t *can_item)
{
    LOG_INFO("msg type:%ld\n", can_item->type);
    LOG_INFO("can_item->channel:%lu\n", can_item->channel);
    LOG_INFO("can_item->id:%lx\n", can_item->id);
    LOG_INFO("can_item->dlc:%lu\n", can_item->dlc);
    LOG_INFO("data : ");
    for (int i = 0; i < can_item->dlc; i++)
    {
        printf("%x ", can_item->data[i]);
    }
    printf("\n");
}

void microseconds_sleep(unsigned long uSec)
{
    struct timeval tv;
    tv.tv_sec = uSec / 1000000;
    tv.tv_usec = uSec % 1000000;
    int err;
    do
    {
        err = select(0, NULL, NULL, NULL, &tv);
    } while (err < 0 && errno == EINTR);
}
/*************************************************
Function:     deal_can_frame
Description:  将读到的内容尝试解析为结构体
Return: 
Others: -1 解析失败 0 解析成功
**************************************************/
int deal_can_frame(char *buff, can_item_t *can_item)
{
    char *delim = " ";
    char *res = NULL;
    int index = 0;
    uint64_t time = 0;
    uint32_t can_frame_flag = 0;

    res = strtok(buff, delim);
    time = str2time(res, can_item);
    can_item->timestamp = time;
    while (res != NULL)
    {
        if (index == INDEX_CHANNEL)
        {
            can_item->channel = atoi(res);
            //LOG_INFO("channel:%ld\n", can_item->channel);
        }
        else if (index == INDEX_ID)
        {
            if (strstr(res, "Error") != NULL)
            {
                //todo:填充错误
                can_item->id = 0xffffffffffffffff;
                can_item->dlc = 0;

                return 0;
            }
            else
            {
                char *p = res;
                can_item->id = (uint64_t)strtol(p, NULL, 16);
            }
        }
        else if (index == INDEX_D_FLAG)
        {
            if (strcmp(res, "d") == 0)
                can_frame_flag = 1;
            else
                return -1;
        }
        else if (index == INDEX_DLC)
            can_item->dlc = atoi(res);
        else if (index >= INDEX_DATA && index <= (can_item->dlc + 6))
        {
            char *p = res;
            can_item->data[index - INDEX_DATA] = (char)strtol(p, NULL, 16);
        }
        res = strtok(NULL, delim);
        index++;
    }

    if (can_frame_flag == 0)
    {
        return -1;
    }
    return 0;
}
/*************************************************
Function:     send_can_to_vci
Description:  将数据发送到sysbus上
Return: 
**************************************************/
void send_can_to_vci(can_item_t *can_item)
{
}
void event_sighandler(int signo)
{
}
/*************************************************
Function:     send_thread
Description:  发送can数据到vci8
Return: 
**************************************************/
void *send_thread(void *argv)
{
    int *index = (int *)argv;
    int msg_len = 0;
    char buf[128];
    can_item_t *can_item = (can_item_t *)buf;
    uint64_t last_frame_time = 0;
    uint64_t base_time = 0;
    uint64_t sleep_time = 0;
    uint64_t speed_time = 0;
    uint64_t fisrt_frame_time = 0;
    char log_buf[128];
    while (1)
    {
        msg_len = msgrcv(g_msqid, can_item, sizeof(buf), g_vci_item[*index].msg_type, 0);
        if (msg_len < 0)
        {
            continue;
        }
        if (g_vci_item[*index].refresh_base_time == NEED_REFRESH_BSAE_TIME)
        {
            sprintf(log_buf, "refresh_base_time\n");
            fwrite(log_buf, 1, strlen(log_buf), g_vci_item[*index].fp);
            last_frame_time = can_item->timestamp;
            g_vci_item[*index].refresh_base_time = NO_NEED_REFRESH_BSAE_TIME;
            base_time = get_time_us() - can_item->timestamp;
        }
        speed_time = get_time_us() - base_time;
        if (speed_time >= can_item->timestamp)
        {
            sleep_time = 0;
            sprintf(log_buf, "over:%ld,can_timestamp:%ld, last_frame_time:%ld\n", speed_time - can_item->timestamp, can_item->timestamp, last_frame_time);
            fwrite(log_buf, 1, strlen(log_buf), g_vci_item[*index].fp);
        }
        else
        {
            sleep_time = can_item->timestamp - speed_time;
            microseconds_sleep(sleep_time);
            sprintf(log_buf, "sleep_time:%ld\n", sleep_time);
            fwrite(log_buf, 1, strlen(log_buf), g_vci_item[*index].fp);
        }
        last_frame_time = can_item->timestamp;
    }
}
/*************************************************
Function:     parse_thread
Description:  解析asc文件中的can数据
Return: 
**************************************************/
void *parse_thread(void *argv)
{
    int count = 0;
    FILE *fp = NULL;
    int size = 1024;
    char *buff = NULL;
    can_item_t *can_item;
    uint64_t current_time;
    uint64_t can_item_time = 0;
    int msg_len;
    int ret = 0;
    int i = 0;
    int sum = 0;
    struct msqid_ds info;

    cmd_msg_t *cmd_msg = (cmd_msg_t *)argv;
    can_item = (can_item_t *)g_data_buf;
    LOG_INFO("parse_thread");
    if ((access(cmd_msg->file_name, F_OK)) != 0)
    {
        LOG_ERROR("file not exist\n");
        return NULL;
    }

    if ((fp = fopen(cmd_msg->file_name, "r")) == NULL)
    {
        printf("%s open failed\n", cmd_msg->file_name);
    }

    g_server_info.server_state = STATE_START;

    if ((buff = (char *)malloc(size)) == NULL)
    {
        printf("malloc failed\n");
    }
    current_time = get_time_us();
    //根据start_time 时间进行睡眠
    if (current_time <= (cmd_msg->t_start))
    {
        LOG_INFO("sleep:%ld\n", cmd_msg->t_start - current_time);
        usleep(cmd_msg->t_start - current_time);
    }
    
    while (NULL != fgets(buff, size, fp))
    {
        line_count++;
        ret = deal_can_frame(buff, can_item);
        if (ret != 0)
        {
            LOG_INFO("can frame error,line_count:%d\n", line_count);
            continue;
        }
        if (can_item->timestamp <= cmd_msg->t_beg_offset)
        {
            continue;
        }
        else if (can_item->timestamp > cmd_msg->t_end_offset)
        {
            LOG_INFO("timestamp: %ld\n", can_item->timestamp);
            LOG_INFO("end_offset:%ld\n", cmd_msg->t_end_offset);
            LOG_INFO("arrive end_offset\n");
            break;
        }
        can_item->type = can_item->channel;
        msg_len = sizeof(can_item_t) + can_item->dlc - sizeof(long);
        //将数据写入到消息队列
        //设置阻塞
        if (g_server_info.server_state == STATE_PAUSE)
        {
            cmd_msg->t_cur_offset = can_item->timestamp / 1000;
            sendto(g_server_info.sock_fd, (char *)cmd_msg, sizeof(cmd_msg) + strlen(cmd_msg->file_name), 0, (struct sockaddr *)&g_server_info.client_info.addr, sizeof(struct sockaddr_in)); //将resume时间发送到客户端
            LOG_INFO("resume time:%ld\n", can_item->timestamp);
            sem_wait(&g_server_info.pause_sem);
        }
        if(can_item->channel != 1)
            continue;
        ret = msgsnd(g_msqid, (void *)can_item, msg_len, 0);
        if (ret != 0)
        {
            perror("msgsnd error");
        }
    }
    current_time = get_time_us();
    LOG_INFO("current_timestamp:%lu\n", current_time);
    free(buff);
    return NULL;
}

void cmd_start_handle(cmd_msg_t *cmd_msg)
{
    int res = 0;
    int i = 0;
    struct msqid_ds info;
    int flag;
    uint64_t current_time;
    char file_name[20];

    LOG_INFO("file_name:%s\n", cmd_msg->file_name);

    current_time = get_time_us();
    if (g_server_info.server_state != STATE_IDEL)
    {
        LOG_INFO("status error, current ststus :%d\n", (int)g_server_info.server_state);
        return;
    }
    sem_init(&g_server_info.pause_sem, 0, 0);

    g_msqid = msgget(IPC_PRIVATE, 0666);
    if (g_msqid == -1)
    {
        if (errno == ENOENT)
        {
            g_msqid = msgget(IPC_PRIVATE, IPC_CREAT | 0666);
        }
        if (g_msqid == -1)
            return;
    }
    flag = msgctl(g_msqid, IPC_STAT, &info);
    if (flag < 0)
    {
        perror("get message status error");
        return;
    }
    LOG_INFO("info.msg_qbytes:%ld\n", info.msg_qbytes);

    info.msg_qbytes = MAX_MSG_BYTES;
    flag = msgctl(g_msqid, IPC_SET, &info);
    if (flag < 0)
    {
        perror("set message error");
        return;
    }
    
    for (i = 0; i < MAX_CHANNEL_NUM; i++)
    {
        g_vci_item[i].channel = i;
        g_vci_item[i].msg_type = i + 1;
        sprintf(file_name, "can%d.log", i + 1);
        g_vci_item[i].fp = fopen(file_name, "w+");
        res = pthread_create(&g_vci_item[i].vci_thread, NULL, send_thread, &g_vci_item[i].channel);
        if (res != 0)
        {
            LOG_ERROR("pthread_create failed\n");
        }
    }
    res = pthread_create(&g_replay_can_thread, NULL, parse_thread, cmd_msg);
    if (res != 0)
    {
        LOG_ERROR("pthread_create failed\n");
    }
}

void cmd_stop_handle(cmd_msg_t *cmd_msg)
{
    int res = 0;
    void *ret;
    int i = 0;
    int flag;
    if(g_server_info.server_state != STATE_PAUSE && g_server_info.server_state != STATE_START)
    {
        LOG_INFO("status error, current ststus :%d\n", (int)g_server_info.server_state);
        return ;
    }
    LOG_INFO("current ststus :%d\n", (int)g_server_info.server_state);
    char log_buf[128];

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

    for (int i = 0; i < MAX_CHANNEL_NUM; i++)
    {
        res = pthread_cancel(g_vci_item[i].vci_thread);
        if (res != 0)
        {
            LOG_ERROR("pthread_cancle failed\n");
        }
        res = pthread_join(g_vci_item[i].vci_thread, &ret);
        if (res != 0)
        {
            LOG_ERROR("pthread_join\n");
        }
        if (ret == PTHREAD_CANCELED)
        {
            g_vci_item[i].vci_thread = 0;
        }
        else
            LOG_ERROR("thread %d was not cancle\n", i);

        sprintf(log_buf, "close:%ld\n", g_vci_item[i].channel);
        fwrite(log_buf, 1, strlen(log_buf), g_vci_item[i].fp);
        fclose(g_vci_item[i].fp);
    }
    flag = msgctl(g_msqid, IPC_RMID, NULL);
    if (flag < 0)
    {
        LOG_ERROR("rm message queue error");
        return;
    }
    g_server_info.server_state = STATE_IDEL;
}
void cmd_pause_handle(cmd_msg_t *cmd_msg)
{
    if (g_server_info.server_state != STATE_START)
    {
        LOG_INFO("status error, current ststus :%d\n", (int)g_server_info.server_state);
        return;
    }
    g_server_info.server_state = STATE_PAUSE;
}
void cmd_resume_handle(cmd_msg_t *cmd_msg)
{
    int i = 0;
    if (g_server_info.server_state != STATE_PAUSE)
    {
        LOG_INFO("status error, current ststus :%d\n", (int)g_server_info.server_state);
        return;
    }
    g_server_info.server_state = STATE_START;
    for(i = 0; i < MAX_CHANNEL_NUM; i++)
    {
        g_vci_item[i].refresh_base_time = NEED_REFRESH_BSAE_TIME;
    }
    sem_post(&g_server_info.pause_sem);
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
        cmd_pause_handle(cmd_msg);
        break;
    case CMD_RESUME:
        LOG_INFO("CMD_RESUME\n");
        cmd_resume_handle(cmd_msg);
        break;
    default:
        LOG_WARN("illegal cmd\n");
        break;
    }
}
void convert_ms_to_us(cmd_msg_t *cmd_msg)
{
    cmd_msg->t_start *= 1000;
    cmd_msg->t_resume *= 1000;
    cmd_msg->t_beg_offset *= 1000;
    cmd_msg->t_cur_offset *= 1000;
    cmd_msg->t_end_offset *= 1000;
}
int main(int argc, char **argv)
{
    cmd_msg_t *cmd_msg;
    if (argc != 3)
    { //进程 ip地址, 端口号
        printf("Usage ./server [sysbus] [port]\n");
        return 1; //退出码,表示程序跑完,结果出错
    }
    g_server_info.sock_fd = socket(AF_INET, SOCK_DGRAM, 0); //1.哪个协议族，面向数据报
    if (g_server_info.sock_fd < 0)
    { //文件描述符被占用,出错;一个进程的文件描述符最多为ulimit -a
        perror("socket");
        return 2;
    }
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;                 //协议族
    addr.sin_addr.s_addr = inet_addr(IP_ADDR); //将10进制IP地址转换成unint_32ip
    addr.sin_port = htons(atoi(argv[2]));      //将端口号转成整数，在将其转成字节序

    //将文件描述符进行绑定,其实是将端口号绑定
    int ret = bind(g_server_info.sock_fd, (struct sockaddr *)&addr, sizeof(addr)); //指针强转
    if (ret < 0)
    { //如果端口号被其他进程占用，会失败
        perror("bind");
        return 3;
    }

    g_bmr_rec_handle = sysbus_connect(argv[1], (char *)"can_replay", bmr_rec_handle, NULL);
    while (1)
    {
        struct sockaddr_in peer;
        socklen_t len = sizeof(peer);

        ssize_t read_size = recvfrom(g_server_info.sock_fd, g_cmd_buf, sizeof(g_cmd_buf), 0,
                                     (struct sockaddr *)&peer, &len);
        if (read_size < 0)
        {
            perror("recvfrom");
            continue;
        }
        g_cmd_buf[read_size] = '\0';

        cmd_msg = (cmd_msg_t *)g_cmd_buf;
        convert_ms_to_us(cmd_msg);
        memcpy((char *)&g_server_info.cmd_msg, (char *)cmd_msg, sizeof(cmd_msg_t));

        // LOG_INFO("start:%ld\n", g_server_info.cmd_msg.t_start);
        // LOG_INFO("resume:%ld\n", g_server_info.cmd_msg.t_resume);
        // LOG_INFO("beg_offset:%ld\n", g_server_info.cmd_msg.t_beg_offset);
        // LOG_INFO("cur_offset:%ld\n", g_server_info.cmd_msg.t_cur_offset);
        // LOG_INFO("end_offset:%ld\n", g_server_info.cmd_msg.t_end_offset);

        g_server_info.client_info.addr.sin_addr = peer.sin_addr;
        g_server_info.client_info.addr.sin_port = peer.sin_port;

        cmd_handle(cmd_msg);
    }
    close(g_server_info.sock_fd); //一般执行不到
    return 0;
}