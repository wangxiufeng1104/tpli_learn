#ifndef _QUE_H_
#define _QUE_H_

#include <sys/types.h>   //for key_t

/**
 * @param[in] qid
 * @param[in] pdata
 * @param[in] len
 * @param[in] timeout -1 block ,0 nonblock, other wait timeout ms
 * @return len if successful, -1 error
*/
int send_que_timedwait(key_t qid,unsigned char *pdata,size_t len,int timeout);

/**
 * @param[in] qid
 * @param[in] pdata
 * @param[in] len
 * @param[in] timeout -1 block ,0 nonblock, other wait timeout ms
 * @return >0 if successful, -1 error
*/
int read_que_timedwait(key_t qid,unsigned char *pdata,size_t len,int timeout);

#endif
