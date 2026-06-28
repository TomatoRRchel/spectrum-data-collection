#ifndef IPC_CLIENT_H
#define IPC_CLIENT_H

#include <stdbool.h>
#include <sys/types.h>
#include "ipc_mgr.h"

// 客户端句柄
typedef struct
{
    int sock_fd;
    pid_t pid;
    client_type_t type;
    bool registered;
    // name_t name;
    // 获取到的 IPC 信息
    // key_t shm_key;
    // int shm_id;
    // size_t shm_size;
    // key_t sem_key;
    int sem_id;
    void *shm_ptr; // 附加后的地址
} ipc_client_t;

// 初始化客户端，连接守护进程并注册
// type: CLIENT_LONG_TERM 或 CLIENT_TRANSIENT
int ipc_client_init(ipc_client_t *client, client_type_t type);

// 获取共享内存指针（内部会完成附加）
void *ipc_client_get_shm(ipc_client_t *client);

// 获取信号量 ID
int ipc_client_get_semid(ipc_client_t *client);

// 发送心跳（长期进程必须周期性调用，例如每2秒一次）
int ipc_client_heartbeat(ipc_client_t *client);

// 关闭客户端，注销并清理资源
void ipc_client_close(ipc_client_t *client);

#endif