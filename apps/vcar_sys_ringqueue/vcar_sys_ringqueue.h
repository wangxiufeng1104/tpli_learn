#ifndef _VCAR_SYS_RINGQUEUE_H_
#define _VCAR_SYS_RINGQUEUE_H_

#ifdef __cplusplus
extern "C" {
#endif
#define VCAR_SYS_RQ_OK							0
#define VCAR_SYS_RQ_ERR_INPUT						-1
#define VCAR_SYS_RQ_ERR_FULL						-2
#define VCAR_SYS_RQ_ERR_AGAIN						-3
#define VCAR_SYS_RQ_ERR_INVALID					-4


int64_t VCAR_SYS_RingQueueCreat(int memSize, int queueSize, unsigned int mask);
int VCAR_SYS_RingQueueClean(int64_t queue);
int VCAR_SYS_RingQueueDestroy(int64_t queue);



int VCAR_SYS_RingQueueWrite(int64_t queue, char* data, int dataLen, char *header, int headerLen);
int VCAR_SYS_RingQueueRead(int64_t queue, int64_t *ringIndex, unsigned int mask, char* data, int dataLen, char *header, int headerLen);
int VCAR_SYS_RingQueueReadFromHead(int64_t queue, int64_t *ringIndex, unsigned int mask, char* data, int dataLen, char *header, int headerLen);

typedef int (*VerifyFunc )(char *header, int headeLen, void *para);
int VCAR_SYS_RingQueueFind(int64_t queue, int64_t *ringIndex, unsigned int mask, VerifyFunc verify, char* data, int dataLen, char *header, int headerLen, void *para);




int VCAR_SYS_RingQueueGetMask(int64_t queue);
int VCAR_SYS_RingQueueSetMask(int64_t queue, unsigned int mask);
int VCAR_SYS_RingQueueClearMask(int64_t queue, unsigned int mask);
int VCAR_SYS_RingQueueGetNextValidIndex(int64_t queue, int64_t *ringIndex);
int VCAR_SYS_RingQueueFindPost(int64_t queue, int64_t *ringIndex, unsigned int mask, VerifyFunc verify, char* data, int dataLen, char *header, int headerLen, void *para);

int VCAR_SYS_RingQueueGetHeader(int64_t queue, int64_t *ringIndex);
int VCAR_SYS_RingQueueGetTail(int64_t queue, int64_t *ringIndex);

#ifdef __cplusplus
}
#endif
#endif

