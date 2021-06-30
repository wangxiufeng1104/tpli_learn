#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include <vcar_sys_ringqueue.h>
int32_t memSize = 1024;
int32_t queueSize = 100;
int64_t ringqueue = 0;
#pragma  pack(1)
typedef struct 
{
    uint8_t cmd;                         // 0 - start record; 1 - stop record; 2 - config video
    uint8_t seg_mode;                    // 0 - size based segment; 1 - time based segment. if cmd is 2, ignore
    uint32_t seg_value;                  // if size based, unit is MB, if time based, unit is min. if cmd is 2, ignore
    uint16_t file_name_len;              // if cmd is 0 and 1, strlen(file_name); if cmd is 2, sizeof(sysbus_cfg_video_t)
    uint64_t timestamp;                  // if cmd is 2, ignore 
    uint8_t data[0];                     // variable length includ file_name string and tag string.
} sysbus_rec_cmd_msg_t;
#pragma  pack()


#define START 0
#define STOP 1
int write_ringqueue(int64_t queue, sysbus_rec_cmd_msg_t *cmd_msg)
{
    if (!cmd_msg || queue == 0)
    {
        return VCAR_SYS_RQ_ERR_INPUT;
    }
    int ret;
    ret = VCAR_SYS_RingQueueWrite(queue, (char *)cmd_msg->data, cmd_msg->file_name_len, (char *)cmd_msg, sizeof(sysbus_rec_cmd_msg_t));
    return ret;
}
int read_ringqueue(int64_t queue, int64_t *MsgIndex, sysbus_rec_cmd_msg_t *msg, int32_t datalen)
{
    if (!msg || !MsgIndex)
    {
        printf("input args error\n");
        return VCAR_SYS_RQ_ERR_INPUT;
    }
    int ret;
    ret = VCAR_SYS_RingQueueReadFromHead(queue, MsgIndex, 0, (char *)msg->data, datalen, (char *)msg, sizeof(sysbus_rec_cmd_msg_t));
    return ret;
}

sysbus_rec_cmd_msg_t *cmd_buf;
char writebuf[100];
char readbuf[100];
char filename[20];


int produce_num = 0;
void produce(void)
{
    
    memset(filename, 0, sizeof(filename));
    sprintf(filename, "%s_%d", "cmd_index", produce_num);
    cmd_buf = (sysbus_rec_cmd_msg_t *)writebuf;
    cmd_buf->cmd = produce_num%2;
    cmd_buf->file_name_len = strlen(filename);
    memcpy(cmd_buf->data, filename, strlen(filename));
    write_ringqueue(ringqueue, cmd_buf);
    produce_num ++;
}
void produce_one(int argv)
{
    printf("prodecu 1\n");
    produce();
}
void produce_two(int argv)
{
    printf("prodecu 2\n");
    produce();
    produce();
}
void *customer(void *argv)
{
    int64_t head, tail;
    VCAR_SYS_RingQueueGetHeader(ringqueue, &head);
    while (1)
    {
        VCAR_SYS_RingQueueGetTail(ringqueue, &tail);
        while (head != tail)
        {
            sysbus_rec_cmd_msg_t *data = (sysbus_rec_cmd_msg_t *)readbuf;
            read_ringqueue(ringqueue, &head,data, sizeof(readbuf));
            printf("ringINdex:%lx\t", head);
            printf("filename:%s\t", data->data);
            printf("cmd:%s\n", (data->cmd == START) ? "start":"stop");
        }
        usleep(100);
    }
}
int main(int argc, char **argv)
{
    ringqueue = VCAR_SYS_RingQueueCreat(memSize, queueSize, 0);
    
    pthread_t tid;
    signal(SIGUSR1, produce_one);
    signal(SIGUSR2, produce_two);

    pthread_create(&tid, NULL, customer, NULL);
    
    while(1)
    {
        sleep(1);
    }
    return 0;
}