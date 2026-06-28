#ifndef PRODUCER_H
#define PRODUCER_H

#include <pthread.h>
#include <semaphore.h>
#include "shm_ipc.h"
#include "idefine_fun.h"

// 工作模式枚举
#define ERROR -1
#define MODE_POWER_SPECTRUM 0
#define MODE_IQ 1

// 传递给生产者线程的参数结构
typedef struct
{
    SpectrumData *shm;      // 共享内存指针
    int sem_id;             // System V 信号量ID（保护共享内存）
    file_Manager *file_mgr; // 文件管理器指针
    sem_t *upload_sem;      // 触发消费者上传的POSIX信号量
    atomic_bool *shutdown;  // 退出标志
} ProducerArgs;

// 生产者线程入口函数
void *spectrum_thread(void *arg);

#endif // PRODUCER_H