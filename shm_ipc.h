#ifndef SPECTRUM_IPC_H
#define SPECTRUM_IPC_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
// 共享内存数据结构
typedef struct
{
    double start_freq;      // 起始频率 (Hz)
    double stop_freq;       // 中止频率 (Hz)
    int mode;               // 工作模式（0=功率谱，1=IQ）
    unsigned int ref_count; // 参考计数（可用于版本控制）
    pid_t controller_pid;   // 生产者进程ID
    pid_t spectrum_pid;     // 消费者进程ID
    uint32_t sequence;      // 数据序列号
    time_t timestamp;       // 更新时间戳
    time_t heartbeat;       // 生产者心跳时间戳
    uint8_t data_ready;     // 数据就绪标志
    uint8_t shutdown;       // 关闭标志
} SpectrumData;

// 共享内存和信号量键值
#define SHM_KEY 0x1234
#define SEM_KEY 0x5678

// 信号定义
#define SIG_DATA_READY SIGUSR1
#define SIG_SHUTDOWN SIGUSR2
// #define MODE_POWER_SPECTRUM 0
// #define MODE_IQ 1

typedef enum
{
    PRODUECR = 0,
    CONSUMER,
    USER_INTERFACE
} Role;

// 函数声明
int init_shared_memory(bool *out_is_new);
void ipc_ref_inc(SpectrumData *shm, int sem_id);
int ipc_ref_dec(SpectrumData *shm, int sem_id);
void *attach_shared_memory(void);
int detach_shared_memory(void *shm_ptr);
int destroy_shared_memory(void);
int init_semaphore(bool *out_is_new);
int lock_semaphore(int sem_id);
int unlock_semaphore(int sem_id);
int destroy_semaphore(int sem_id);

#endif
