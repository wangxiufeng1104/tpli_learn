#include <pthread.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include "vcar_sys_ringqueue.h"


#define VCAR_SYS_RQ_MAX_HEADER_LEN			128


typedef struct S_VCAR_SYS_RingQueueItemInfo{
    pthread_rwlockattr_t rwattr;
    pthread_rwlock_t rwlock;
    uint32_t mask;
    unsigned char valid;
    char header[VCAR_SYS_RQ_MAX_HEADER_LEN];
    int32_t headerLen;
    int32_t dataLen;
    char *data;
    struct S_VCAR_SYS_RingQueueItemInfo *next;
    struct S_VCAR_SYS_RingQueueItemInfo *prev;
}VCAR_SYS_RingQueueItemInfo;

typedef struct S_VCAR_SYS_RingQueue{
    uint32_t userMask;
    uint32_t initMask;
    int32_t memSize;
    char *memStart;
    char *memEnd;
    char *wrPos;
    char *wrEnd;
    int32_t queueSize;
    VCAR_SYS_RingQueueItemInfo *start;
    VCAR_SYS_RingQueueItemInfo *head;
    VCAR_SYS_RingQueueItemInfo *tail;
    pthread_rwlockattr_t rwattr;
    pthread_rwlock_t rwlock;
}VCAR_SYS_RingQueue;

int32_t VCAR_SYS_RWLockInit(pthread_rwlock_t* rwlock, pthread_rwlockattr_t* attr) 
{
    if(NULL == rwlock)
    {      
        return -1;
    }
    if(attr)
    {   
        if(pthread_rwlockattr_init(attr))
        {
            return -1;       
        }
        pthread_rwlockattr_setkind_np(attr,PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);                                                                                                                    
    }
    if(pthread_rwlock_init(rwlock,attr))
    {                                                                                                                                                                    
        if(attr)
        {
            pthread_rwlockattr_destroy(attr);                                                                                                                                                                
        }
        return -1;
    }
    return 0;
}


int32_t VCAR_SYS_RWLockDestroy(pthread_rwlock_t* rwlock, pthread_rwlockattr_t* attr)
{
    if(rwlock)
    {
        pthread_rwlock_destroy(rwlock);
    }
    if(attr)
    {
        pthread_rwlockattr_destroy(attr);
    }
    return 0;
}


int32_t VCAR_SYS_RingQueueDestroy(int64_t queue)
{
    VCAR_SYS_RingQueue *pQueue = (VCAR_SYS_RingQueue *)queue;
    VCAR_SYS_RingQueueItemInfo *pItem = NULL;
    int32_t i = 0;

    if(pQueue->start)
    {
        pItem = pQueue->start;
        for(i = 0; i < pQueue->queueSize; i++)
        {
            VCAR_SYS_RWLockDestroy(&(pItem->rwlock), &(pItem->rwattr));
            pItem++;
        }

        free(pQueue->start);
    }
    if(pQueue->memStart)
    {
        free(pQueue->memStart);
    }

    VCAR_SYS_RWLockDestroy(&(pQueue->rwlock), &(pQueue->rwattr));
    free(pQueue);
    return VCAR_SYS_RQ_OK;
}
int32_t VCAR_SYS_RingQueueClean(int64_t queue)
{
    VCAR_SYS_RingQueue *pQueue = (VCAR_SYS_RingQueue *)&queue;
    VCAR_SYS_RingQueueItemInfo *pItem = NULL;
    int32_t i = 0;

    pthread_rwlock_wrlock(&pQueue->rwlock);
    pQueue->wrPos = pQueue->memStart;
    pQueue->wrEnd = pQueue->memEnd;
    pQueue->head = pQueue->tail = pQueue->start;
    pQueue->userMask = pQueue->initMask;
    pQueue->head->valid = 0;

    pItem = &(pQueue->start[0]);
    for(i = 0; i < pQueue->queueSize; i++)
    {
        pItem->valid = 0;		
        pItem++;
    }
    pthread_rwlock_unlock(&pQueue->rwlock);
    return 0;
}

int64_t VCAR_SYS_RingQueueCreat(int32_t memSize, int32_t queueSize, uint32_t mask)
{
    VCAR_SYS_RingQueue *pQueue = (VCAR_SYS_RingQueue *)malloc(sizeof(VCAR_SYS_RingQueue));
    VCAR_SYS_RingQueueItemInfo *pItem = NULL;
    int32_t i = 0;

    if(memSize <= 0 || queueSize <= 1)
    {
        goto EXIT;
    }
    if(!pQueue)
    {
        goto EXIT;
    }

    memset(pQueue, 0, sizeof(VCAR_SYS_RingQueue));
    pQueue->memStart = (char*)malloc(memSize);
    if(!pQueue->memStart)
    {
        goto EXIT;
    }

    pQueue->start = (VCAR_SYS_RingQueueItemInfo*)malloc(queueSize*sizeof(VCAR_SYS_RingQueueItemInfo));
    if(!pQueue->start)
    {
        goto EXIT;
    }

    pQueue->memEnd = pQueue->memStart + memSize;
    pQueue->wrPos = pQueue->memStart;
    pQueue->wrEnd = pQueue->memEnd;

    pQueue->queueSize = queueSize;
    memset(pQueue->start, 0, queueSize*sizeof(VCAR_SYS_RingQueueItemInfo));

    if(mask)
    {
        pQueue->userMask = mask;
        pQueue->initMask = mask;
    }

    pItem = pQueue->start;
    pItem->prev = &(pQueue->start[pQueue->queueSize-1]);

    VCAR_SYS_RWLockInit(&(pItem->rwlock), &(pItem->rwattr));
    if(pQueue->queueSize > 1)
    {
        pItem->next = &(pQueue->start[1]);
        pItem = pItem->next;
        for(i = 1; i < (pQueue->queueSize - 1); i++)
        {
            pItem->prev = &(pQueue->start[i-1]);
            pItem->next = &(pQueue->start[i+1]);
            if(VCAR_SYS_RWLockInit(&(pItem->rwlock), &(pItem->rwattr))!= 0)
            {
                goto EXIT;
            }			
            pItem = pItem->next;
        }
        pItem->prev = &(pQueue->start[pQueue->queueSize-2]);
        pItem->next = &(pQueue->start[0]);
        VCAR_SYS_RWLockInit(&(pItem->rwlock), &(pItem->rwattr));
    }
    else
    {
        pItem->next = &(pQueue->start[0]);
    }

    pQueue->head = pQueue->tail = pQueue->start;
    VCAR_SYS_RWLockInit(&(pQueue->rwlock), &(pQueue->rwattr));
    return (int64_t)pQueue;
EXIT:
    if(pQueue)
    {
        VCAR_SYS_RingQueueDestroy((int64_t)pQueue);
    }
    return 0;	
}

static int32_t setItemTail(VCAR_SYS_RingQueue *pQueue, char *data, int32_t dataLen, char *header, int32_t headerLen)
{
    pthread_rwlock_wrlock(&(pQueue->tail->rwlock));
    pQueue->tail->data = pQueue->wrPos;
    memcpy(pQueue->tail->data, data, dataLen);
    pQueue->tail->dataLen = dataLen;
    pQueue->tail->mask = pQueue->userMask;
    if(header && headerLen > 0)
    {
        memcpy(pQueue->tail->header, header, headerLen);
        pQueue->tail->headerLen = headerLen;
    }
    else
    {
        pQueue->tail->headerLen = headerLen = 0;
    }
    pQueue->tail->valid = 1;
    pthread_rwlock_unlock(&(pQueue->tail->rwlock));

    pthread_rwlock_wrlock(&(pQueue->rwlock));
    pQueue->tail = pQueue->tail->next;
    pQueue->wrPos += dataLen;
    pthread_rwlock_unlock(&(pQueue->rwlock));

    return 0;
}
static int32_t isItemHeaderClean(VCAR_SYS_RingQueue *pQueue)
{
    pthread_rwlock_wrlock(&pQueue->head->rwlock);
    if(pQueue->head->mask)
    {				
        pthread_rwlock_unlock(&pQueue->head->rwlock);
        return 0;
    }
    pthread_rwlock_unlock(&pQueue->head->rwlock);
    return 1;
}
static int32_t moveItemHeader(VCAR_SYS_RingQueue *pQueue)
{
    pthread_rwlock_wrlock(&pQueue->rwlock);
    pQueue->head->valid = 0;
    pQueue->head = pQueue->head->next;		
    pthread_rwlock_unlock(&pQueue->rwlock);
    return 0;
}


int32_t VCAR_SYS_RingQueueWrite(int64_t queue, char* data, int32_t dataLen, char *header, int32_t headerLen)
{	

    int32_t ret = VCAR_SYS_RQ_OK;
    int32_t freeMemSize = 0;
    VCAR_SYS_RingQueue *pQueue = (VCAR_SYS_RingQueue *)queue;
    //char* frameEnd = 0;
    //VS_RQ_DEBUG("VCAR_SYS_RingQueueWrite in, dataLen=%d\n", dataLen);
    if(!data || dataLen < 0 || (header && headerLen > VCAR_SYS_RQ_MAX_HEADER_LEN))
    {
        ret = VCAR_SYS_RQ_ERR_INPUT;
        goto ERR_EXIT;
    }

CHECK_MEM:
    freeMemSize = (pQueue->wrEnd - pQueue->wrPos);
    //VS_RQ_DEBUG("freeMemSize=%d,dataLen=%d\n", freeMemSize, dataLen);
    if(pQueue->tail->next == pQueue->head)
    {
        if(!isItemHeaderClean(pQueue))
        {
            ret = VCAR_SYS_RQ_ERR_FULL;
            goto ERR_EXIT;
        }
        moveItemHeader(pQueue);
    }
    //check the free mem size
    if(freeMemSize >= dataLen)
    {
        setItemTail(pQueue, data, dataLen, header, headerLen);
        //printf("pQueue->head=%p, pQueue->tail=%p,!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!write\n", pQueue->head, pQueue->tail);
        //VS_RQ_DEBUG("VCAR_SYS_RingQueueWrite out,dataLen=%d\n", dataLen);
    }
    else
    {
        //VS_RQ_DEBUG("@@@@@@@@@@@@@@@@@@@@@@@@@@@@@need wrap\n");			
        //header is in front of the tail
        if(pQueue->wrPos > pQueue->head->data)
        {
            if(pQueue->wrEnd != pQueue->memEnd)
            {
                pQueue->wrEnd = pQueue->memEnd;
            }
            else
            {
                if(!isItemHeaderClean(pQueue))
                {
                    ret = VCAR_SYS_RQ_ERR_FULL;
                    goto ERR_EXIT;
                }
                pQueue->wrEnd = (pQueue->head->data + pQueue->head->dataLen);
                moveItemHeader(pQueue);
                pQueue->wrPos = pQueue->memStart;
            }
        }
        else
        {
            if(!isItemHeaderClean(pQueue))
            {
                ret = VCAR_SYS_RQ_ERR_FULL;
                goto ERR_EXIT;
            }
            pQueue->wrEnd = (pQueue->head->data + pQueue->head->dataLen);
            moveItemHeader(pQueue);
        }
        goto CHECK_MEM;
    }
ERR_EXIT:
    return ret;

}
static int32_t _VCAR_SYS_RingQueueRead(int64_t queue, int64_t *ringIndex, uint32_t mask, char* data, int32_t dataLen, char *header, int32_t headerLen, int32_t isFromHead)
{
    int32_t ret = VCAR_SYS_RQ_OK;
    int32_t realDataLen = 0;
    int32_t realHeaderLen = 0;
    VCAR_SYS_RingQueue *pQueue = (VCAR_SYS_RingQueue *)queue;
    VCAR_SYS_RingQueueItemInfo *pItem = NULL;


    if(!ringIndex || !data)
    {
        return VCAR_SYS_RQ_ERR_INPUT;
    }
    pthread_rwlock_rdlock(&(pQueue->rwlock));
    if(isFromHead)
    {
        pItem = (0 == *ringIndex) ? pQueue->head : ((VCAR_SYS_RingQueueItemInfo *)(*ringIndex));
        //VS_RQ_DEBUG("pItem=%p, pQueue->head=%p, pQueue->tail=%p, ***************************************************************************************************\n", pItem, pQueue->head, pQueue->tail);
    }
    else
    {
        pItem = (0 == *ringIndex) ? pQueue->tail->prev : ((VCAR_SYS_RingQueueItemInfo *)(*ringIndex));
#if 0
        if(pItem == pQueue->tail)
        {
            //printf("read again\n");
            ret = VCAR_SYS_RQ_ERR_AGAIN;
            pthread_rwlock_unlock(&pQueue->rwlock);
            goto ERR_EXIT;
        }
#endif

    }
#if 1
    if((int64_t)(pItem) == (int64_t)(pQueue->tail))
    {
        //VS_RQ_DEBUG("pItem=%p, pQueue->head=%p, pQueue->tail=%p, @@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@read again\n", pItem, pQueue->head, pQueue->tail);
        ret = VCAR_SYS_RQ_ERR_AGAIN;
        pthread_rwlock_unlock(&pQueue->rwlock);
        goto ERR_EXIT;
    }	
#endif
    pthread_rwlock_rdlock(&(pItem->rwlock));
    pthread_rwlock_unlock(&(pQueue->rwlock));
    if(!pItem->valid)
    {
        //printf("read invalid\n");
        ret = VCAR_SYS_RQ_ERR_INVALID;		
        pthread_rwlock_unlock(&pItem->rwlock);
        goto ERR_EXIT;
    }

    if(pItem->mask & mask)
    {
        pItem->mask &= (~mask);
    }
    if(data)
    {
        realDataLen = (dataLen > pItem->dataLen) ? pItem->dataLen:dataLen;
        memcpy(data, pItem->data, realDataLen);
    }

    if(header && headerLen > 0)
    {
        realHeaderLen = (headerLen > pItem->headerLen) ? pItem->headerLen :headerLen;
        memcpy(header, pItem->header, realHeaderLen);
    }

    *ringIndex = (int64_t)(pItem->next);
    pthread_rwlock_unlock(&pItem->rwlock);
ERR_EXIT:
    return ret;	
}


int32_t VCAR_SYS_RingQueueFind(int64_t queue, int64_t *ringIndex, uint32_t mask, VerifyFunc verify, char* data, int32_t dataLen, char *header, int32_t headerLen, void *para)
{
    int32_t realDataLen = 0;
    int32_t realHeaderLen = 0;
    VCAR_SYS_RingQueue *pQueue = (VCAR_SYS_RingQueue *)queue;
    VCAR_SYS_RingQueueItemInfo *pItem = NULL;


    if(!ringIndex || !verify)
    {
        return VCAR_SYS_RQ_ERR_INPUT;
    }
    pthread_rwlock_rdlock(&pQueue->rwlock);
    pItem = (0 == *ringIndex) ? pQueue->tail->prev : ((VCAR_SYS_RingQueueItemInfo *)(*ringIndex));
    while(pItem->valid && pItem != pQueue->head->prev)
    {
        if(verify(pItem->header, pItem->headerLen, para))
        {
            if(pItem->mask & mask)
            {
                pItem->mask &= (~mask);
            }
            if(data)
            {
                realDataLen = (dataLen > pItem->dataLen) ? pItem->dataLen:dataLen;
                memcpy(data, pItem->data, realDataLen);
            }

            if(header && headerLen > 0)
            {
                realHeaderLen = (headerLen > pItem->headerLen) ? pItem->headerLen :headerLen;
                memcpy(header, pItem->header, realHeaderLen);
            }
            *ringIndex = (int64_t)(pItem->next);
            pthread_rwlock_unlock(&pQueue->rwlock);
            return VCAR_SYS_RQ_OK;
        }
        else
        {
            pItem = pItem->prev;
        }
    }
    pthread_rwlock_unlock(&pQueue->rwlock);
    return VCAR_SYS_RQ_ERR_AGAIN;	
}

int32_t VCAR_SYS_RingQueueFindPost(int64_t queue, int64_t *ringIndex, uint32_t mask, VerifyFunc verify, char* data, int32_t dataLen, char *header, int32_t headerLen, void *para)
{
    int32_t realDataLen = 0;
    int32_t realHeaderLen = 0;
    VCAR_SYS_RingQueue *pQueue = (VCAR_SYS_RingQueue *)queue;
    VCAR_SYS_RingQueueItemInfo *pItem = NULL;


    if(!ringIndex || !verify)
    {
        return VCAR_SYS_RQ_ERR_INPUT;
    }
    pthread_rwlock_rdlock(&pQueue->rwlock);
    pItem = (0 == *ringIndex) ? pQueue->head : ((VCAR_SYS_RingQueueItemInfo *)(*ringIndex));
    while(pItem != pQueue->tail)
    {
        if(verify(pItem->header, pItem->headerLen, para) && (pItem->valid))
        {
            if(pItem->mask & mask)
            {
                pItem->mask &= (~mask);
            }
            if(data)
            {
                realDataLen = (dataLen > pItem->dataLen) ? pItem->dataLen:dataLen;
                //VS_RQ_DEBUG("realDataLen=%d\n", realDataLen);
                memcpy(data, pItem->data, realDataLen);
            }

            if(header && headerLen > 0)
            {
                realHeaderLen = (headerLen > pItem->headerLen) ? pItem->headerLen :headerLen;
                //VS_RQ_DEBUG("realHeaderLen=%d\n", realHeaderLen);
                memcpy(header, pItem->header, realHeaderLen);
            }
            *ringIndex = (int64_t)(pItem->next);
            pthread_rwlock_unlock(&pQueue->rwlock);
            return VCAR_SYS_RQ_OK;
        }
        else
        {
            *ringIndex = (int64_t)(pItem->next);
            pItem = pItem->next;
        }
    }
    pthread_rwlock_unlock(&pQueue->rwlock);
    return VCAR_SYS_RQ_ERR_AGAIN;	
}


int32_t VCAR_SYS_RingQueueReadFromHead(int64_t queue, int64_t *ringIndex, uint32_t mask, char* data, int32_t dataLen, char *header, int32_t headerLen)
{
    return _VCAR_SYS_RingQueueRead(queue,ringIndex,mask,data,dataLen,header,headerLen,1);
}

int32_t VCAR_SYS_RingQueueRead(int64_t queue, int64_t *ringIndex, uint32_t mask, char* data, int32_t dataLen, char *header, int32_t headerLen)
{	
    return _VCAR_SYS_RingQueueRead(queue,ringIndex,mask,data,dataLen,header,headerLen,0);
}
int32_t VCAR_SYS_RingQueueGetMask(int64_t queue)
{
    VCAR_SYS_RingQueue* pQueue = (VCAR_SYS_RingQueue *)queue;
    uint32_t mask = 0;
    pthread_rwlock_rdlock(&pQueue->rwlock);
    mask = pQueue->userMask;
    pthread_rwlock_unlock(&pQueue->rwlock);
    return mask;
}
int32_t VCAR_SYS_RingQueueSetMask(int64_t queue, uint32_t mask)
{
    VCAR_SYS_RingQueue* pQueue = (VCAR_SYS_RingQueue *)queue;
    if(mask)
    {
        pthread_rwlock_wrlock(&pQueue->rwlock);
        pQueue->userMask |= mask;
        pthread_rwlock_unlock(&pQueue->rwlock);
    }
    return VCAR_SYS_RQ_OK;
}
int32_t VCAR_SYS_RingQueueClearMask(int64_t queue, uint32_t mask)
{
    VCAR_SYS_RingQueue* pQueue = (VCAR_SYS_RingQueue *)queue;
    if(mask)
    {
        pthread_rwlock_wrlock(&pQueue->rwlock);
        pQueue->userMask &= (~mask);
        pthread_rwlock_unlock(&pQueue->rwlock);
    }
    return VCAR_SYS_RQ_OK;

}
int32_t VCAR_SYS_RingQueueGetNextValidIndex(int64_t queue, int64_t *ringIndex)
{
    VCAR_SYS_RingQueue *pQueue = (VCAR_SYS_RingQueue *)queue;
    VCAR_SYS_RingQueueItemInfo *pItem = NULL;

    if(!ringIndex)
    {
        return VCAR_SYS_RQ_ERR_INPUT;
    }
    pthread_rwlock_rdlock(&pQueue->rwlock);
    pItem = (0 == *ringIndex) ? pQueue->head : ((VCAR_SYS_RingQueueItemInfo *)(*ringIndex));
    while(pItem != pQueue->tail)
    {
        if(pItem->valid)
        {
            pthread_rwlock_unlock(&pQueue->rwlock);
            return VCAR_SYS_RQ_OK;
        }
        pItem = pItem->next;
        *ringIndex = (int64_t)(pItem);
    }
    pthread_rwlock_unlock(&pQueue->rwlock);
    return VCAR_SYS_RQ_ERR_AGAIN;
}
int VCAR_SYS_RingQueueGetHeader(int64_t queue, int64_t *ringIndex)
{
    VCAR_SYS_RingQueue *pQueue = (VCAR_SYS_RingQueue *)queue;

    if(!ringIndex)
    {
        return VCAR_SYS_RQ_ERR_INPUT;
    }
    pthread_rwlock_rdlock(&pQueue->rwlock);
    *ringIndex = (int64_t)pQueue->head;
    pthread_rwlock_unlock(&pQueue->rwlock);
    return VCAR_SYS_RQ_OK;
}
int VCAR_SYS_RingQueueGetTail(int64_t queue, int64_t *ringIndex)
{
    VCAR_SYS_RingQueue *pQueue = (VCAR_SYS_RingQueue *)queue;

    if(!ringIndex)
    {
        return VCAR_SYS_RQ_ERR_INPUT;
    }
    pthread_rwlock_rdlock(&pQueue->rwlock);
    *ringIndex = (int64_t)pQueue->tail;
    pthread_rwlock_unlock(&pQueue->rwlock);
    return VCAR_SYS_RQ_OK;
}
