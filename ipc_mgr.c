// ipc_mgr_daemon.c
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/select.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include <pthread.h>
#include "ipc_mgr.h"
#include "shm_ipc.h" // SpectrumData 定义，稍作调整

// ============ 全局变量 ============
static int g_server_fd = -1;
static int g_shm_id = -1;
static int g_sem_id = -1;
static SpectrumData *g_shm_ptr = NULL;
static volatile int g_running = 1;

// 客户端记录
#define MAX_CLIENTS 32
typedef struct
{
    pid_t pid;
    client_type_t type;
    time_t last_heartbeat;
    int sock_fd; // 连接套接字，-1 表示空闲槽
} client_entry_t;

static client_entry_t g_clients[MAX_CLIENTS];
static pthread_mutex_t g_clients_mutex = PTHREAD_MUTEX_INITIALIZER;

// 心跳超时（秒）
#define HEARTBEAT_TIMEOUT 5

// ============ 辅助函数 ============
static void cleanup_ipc(void)
{
    if (g_shm_ptr)
    {
        shmdt(g_shm_ptr);
        g_shm_ptr = NULL;
    }
    if (g_shm_id != -1)
    {
        shmctl(g_shm_id, IPC_RMID, NULL);
        g_shm_id = -1;
    }
    if (g_sem_id != -1)
    {
        semctl(g_sem_id, 0, IPC_RMID);
        g_sem_id = -1;
    }
}

static void signal_handler(int sig)
{
    if (sig == SIGINT || sig == SIGTERM)
    {
        g_running = 0;
    }
}

// 创建 IPC 资源（守护进程启动时调用一次）
static int create_ipc_resources(void)
{
    // 创建共享内存
    g_shm_id = shmget(SHM_KEY, sizeof(SpectrumData), IPC_CREAT | IPC_EXCL | 0666);
    if (g_shm_id < 0)
    {
        perror("shmget");
        return -1;
    }
    g_shm_ptr = (SpectrumData *)shmat(g_shm_id, NULL, 0);
    if (g_shm_ptr == (void *)-1)
    {
        perror("shmat");
        shmctl(g_shm_id, IPC_RMID, NULL);
        return -1;
    }
    memset(g_shm_ptr, 0, sizeof(SpectrumData));

    // 创建信号量
    g_sem_id = semget(SEM_KEY, 1, IPC_CREAT | IPC_EXCL | 0666);
    if (g_sem_id < 0)
    {
        perror("semget");
        shmdt(g_shm_ptr);
        shmctl(g_shm_id, IPC_RMID, NULL);
        return -1;
    }
    union semun
    {
        int val;
    } arg;
    arg.val = 1;
    if (semctl(g_sem_id, 0, SETVAL, arg) == -1)
    {
        perror("semctl");
        semctl(g_sem_id, 0, IPC_RMID);
        shmdt(g_shm_ptr);
        shmctl(g_shm_id, IPC_RMID, NULL);
        return -1;
    }

    printf("IPC resources created: shmid=%d, semid=%d\n", g_shm_id, g_sem_id);
    return 0;
}

// 查找客户端槽位
static int find_client_slot(pid_t pid)
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (g_clients[i].pid == pid && g_clients[i].sock_fd != -1)
        {
            return i;
        }
    }
    return -1;
}

static int add_client(pid_t pid, client_type_t type, int sock_fd)
{
    pthread_mutex_lock(&g_clients_mutex);
    int slot = -1;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (g_clients[i].sock_fd == -1)
        {
            slot = i;
            break;
        }
    }
    if (slot == -1)
    {
        pthread_mutex_unlock(&g_clients_mutex);
        return -1;
    }
    g_clients[slot].pid = pid;
    g_clients[slot].type = type;
    g_clients[slot].last_heartbeat = time(NULL);
    g_clients[slot].sock_fd = sock_fd;
    pthread_mutex_unlock(&g_clients_mutex);
    printf("Client registered: PID=%d, type=%s\n", pid,
           type == CLIENT_LONG_TERM ? "LONG" : "TRANSIENT");
    return 0;
}

static void remove_client_by_pid(pid_t pid)
{
    pthread_mutex_lock(&g_clients_mutex);
    int slot = find_client_slot(pid);
    if (slot != -1)
    {
        close(g_clients[slot].sock_fd);
        g_clients[slot].sock_fd = -1;
        printf("Client unregistered: PID=%d\n", pid);
    }
    pthread_mutex_unlock(&g_clients_mutex);
}

// 更新长期客户端心跳
static void update_heartbeat(pid_t pid)
{
    pthread_mutex_lock(&g_clients_mutex);
    int slot = find_client_slot(pid);
    if (slot != -1 && g_clients[slot].type == CLIENT_LONG_TERM)
    {
        g_clients[slot].last_heartbeat = time(NULL);
    }
    pthread_mutex_unlock(&g_clients_mutex);
}

// 检查心跳超时，移除超时客户端，返回剩余长期进程数
static int check_heartbeat_and_cleanup(void)
{
    time_t now = time(NULL);
    int long_term_count = 0;
    pthread_mutex_lock(&g_clients_mutex);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (g_clients[i].sock_fd != -1)
        {
            if (g_clients[i].type == CLIENT_LONG_TERM)
            {
                if (now - g_clients[i].last_heartbeat > HEARTBEAT_TIMEOUT)
                {
                    printf("Client PID=%d heartbeat timeout, removing\n", g_clients[i].pid);
                    close(g_clients[i].sock_fd);
                    g_clients[i].sock_fd = -1;
                }
                else
                {
                    long_term_count++;
                }
            }
        }
    }
    pthread_mutex_unlock(&g_clients_mutex);
    return long_term_count;
}

// 处理单个客户端请求
static void handle_client(int client_fd)
{
    ipc_request_t req;
    ipc_response_t resp;
    memset(&resp, 0, sizeof(resp));

    ssize_t n = read(client_fd, &req, sizeof(req));
    if (n != sizeof(req))
    {
        // 客户端断开或错误
        return;
    }

    resp.seq = req.seq;

    switch (req.type)
    {
    case REQ_REGISTER:
        if (add_client(req.pid, req.client_type, client_fd) == 0)
        {
            resp.status = RESP_OK;
        }
        else
        {
            resp.status = RESP_ERR_UNKNOWN;
        }
        break;

    case REQ_GET_SHM_INFO:
        // 检查是否已注册（通过pid）
        if (find_client_slot(req.pid) != -1)
        {
            resp.status = RESP_OK;
            resp.shm_key = SHM_KEY;
            resp.shm_id = g_shm_id;
            resp.shm_size = sizeof(SpectrumData);
            resp.sem_key = SEM_KEY;
            resp.sem_id = g_sem_id;
        }
        else
        {
            resp.status = RESP_ERR_NOT_REGISTERED;
        }
        break;

    case REQ_HEARTBEAT:
        update_heartbeat(req.pid);
        resp.status = RESP_OK;
        break;

    case REQ_UNREGISTER:
        remove_client_by_pid(req.pid);
        resp.status = RESP_OK;
        break;

    default:
        resp.status = RESP_ERR_UNKNOWN;
        break;
    }

    write(client_fd, &resp, sizeof(resp));

    // 对于 REGISTER，如果成功，不立即关闭连接，因为后续需要心跳
    // 其他请求处理完即结束，由客户端关闭连接
    if (req.type != REQ_REGISTER)
    {
        // 临时请求，可关闭连接
        // 实际关闭由主循环根据客户端是否持久化处理
    }
}

// 主循环
static void main_loop(void)
{
    fd_set readfds;
    struct timeval tv;
    int max_fd;

    while (g_running)
    {
        FD_ZERO(&readfds);
        FD_SET(g_server_fd, &readfds);
        max_fd = g_server_fd;

        pthread_mutex_lock(&g_clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (g_clients[i].sock_fd != -1)
            {
                FD_SET(g_clients[i].sock_fd, &readfds);
                if (g_clients[i].sock_fd > max_fd)
                {
                    max_fd = g_clients[i].sock_fd;
                }
            }
        }
        pthread_mutex_unlock(&g_clients_mutex);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        int ret = select(max_fd + 1, &readfds, NULL, NULL, &tv);
        if (ret < 0)
        {
            if (errno == EINTR)
                continue;
            perror("select");
            break;
        }

        // 新连接
        if (FD_ISSET(g_server_fd, &readfds))
        {
            struct sockaddr_un addr;
            socklen_t len = sizeof(addr);
            int client_fd = accept(g_server_fd, (struct sockaddr *)&addr, &len);
            if (client_fd >= 0)
            {
                // 新连接不立即处理，等待客户端发送 REGISTER 请求
                // 我们将 client_fd 暂存，但在处理请求时会使用
                // 为简化，这里直接处理第一个请求（假定是REGISTER）
                handle_client(client_fd);
                // 注意：如果是长期客户端，连接会保持，socket fd 已存入 clients 数组
                // 如果是临时客户端，会在处理完请求后由客户端关闭，我们这里不主动关闭
            }
        }

        // 已有客户端数据
        // pthread_mutex_lock(&g_clients_mutex);
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (g_clients[i].sock_fd != -1 && FD_ISSET(g_clients[i].sock_fd, &readfds))
            {
                handle_client(g_clients[i].sock_fd);
            }
        }
        // pthread_mutex_unlock(&g_clients_mutex);
        // for (int i = 0; i < MAX_CLIENTS; i++)
        // {
        //     if (g_clients[i].sock_fd != -1 && FD_ISSET(g_clients[i].sock_fd, &readfds))
        //     {
        //         client_fds[num_clients++] = g_clients[i].sock_fd;
        //     }
        // }
        // 定期检查心跳，并检查是否所有长期进程已退出
        // 定期检查心跳，并检查是否所有长期进程已退出
        // static time_t last_check = 0;
        // time_t now = time(NULL);
        // if (now - last_check >= 1)
        // {
        //     last_check = now;
        //     int long_count = check_heartbeat_and_cleanup();
        //     if (long_count == 0)
        //     {
        //         printf("No long-term clients left, shutting down daemon...\n");
        //         g_running = 0;
        //         break;
        //     }
        // }
    }
}

int main(int argc, char *argv[])
{
    // 初始化信号处理
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    signal(SIGPIPE, SIG_IGN);

    // 创建 IPC 资源
    if (create_ipc_resources() != 0)
    {
        fprintf(stderr, "Failed to create IPC resources\n");
        return 1;
    }
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        g_clients[i].sock_fd = -1;
    }
    // 创建 Unix 域套接字
    struct sockaddr_un addr;
    g_server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_server_fd < 0)
    {
        perror("socket");
        cleanup_ipc();
        return 1;
    }

    unlink(IPC_SOCKET_PATH);
    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, IPC_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(g_server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("bind");
        cleanup_ipc();
        return 1;
    }

    if (listen(g_server_fd, 5) < 0)
    {
        perror("listen");
        cleanup_ipc();
        return 1;
    }

    printf("IPC Manager Daemon started. Socket: %s\n", IPC_SOCKET_PATH);

    main_loop();

    // 清理
    close(g_server_fd);
    unlink(IPC_SOCKET_PATH);
    cleanup_ipc();
    printf("Daemon exited.\n");
    return 0;
}