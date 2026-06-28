#ifndef IPC_MGR_H
#define IPC_MGR_H

#include <stdint.h>
// #include "ipc_client.h"
#define IPC_SOCKET_PATH "/tmp/ipc_mgr_socket"

// 客户端类型
typedef enum
{
    CLIENT_LONG_TERM = 1, // 长期进程（USER1, USER2）
    CLIENT_TRANSIENT = 2  // 临时进程（USER3）
} client_type_t;

// 请求类型
typedef enum
{
    REQ_REGISTER = 1, // 注册客户端
    REQ_GET_SHM_INFO, // 获取共享内存信息（key, id, size, 信号量ID）
    REQ_HEARTBEAT,    // 心跳（长期进程）
    REQ_UNREGISTER,   // 注销客户端
    REQ_SHUTDOWN      // 通知守护进程退出（可选）
} request_type_t;

// 请求结构
typedef struct
{
    request_type_t type;
    client_type_t client_type; // 仅 REGISTER 时有效
    // name_t name;               // 可选，客户端名称（如 "USER1"）
    pid_t pid;    // 客户端 PID
    uint32_t seq; // 序列号，用于匹配响应
} ipc_request_t;

// 响应状态
typedef enum
{
    RESP_OK = 0,
    RESP_ERR_NOT_REGISTERED,
    RESP_ERR_IPC_NOT_READY,
    RESP_ERR_UNKNOWN
} response_status_t;

// 响应结构
typedef struct
{
    uint32_t seq; // 对应请求的序列号
    response_status_t status;
    // 当 type = REQ_GET_SHM_INFO 时有效
    key_t shm_key;
    int shm_id;
    size_t shm_size;
    key_t sem_key;
    int sem_id;
} ipc_response_t;

#endif