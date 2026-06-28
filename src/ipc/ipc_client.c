#include "ipc_client.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <errno.h>
#include <string.h>

static int send_request(ipc_client_t *client, ipc_request_t *req, ipc_response_t *resp)
{

    if (write(client->sock_fd, req, sizeof(*req)) != sizeof(*req))
    {
        return -1;
    }
    if (read(client->sock_fd, resp, sizeof(*resp)) != sizeof(*resp))
    {
        return -1;
    }
    return 0;
}

int ipc_client_init(ipc_client_t *client, client_type_t type)
{
    memset(client, 0, sizeof(*client));
    client->type = type;
    client->pid = getpid();
    client->sock_fd = -1;

    // 连接守护进程
    struct sockaddr_un addr;
    client->sock_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client->sock_fd < 0)
    {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, IPC_SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(client->sock_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
    {
        perror("connect");
        close(client->sock_fd);
        return -1;
    }

    // 注册
    ipc_request_t req = {
        .type = REQ_REGISTER,
        .client_type = type,
        // .name = client->name,
        .pid = client->pid,
        .seq = 1};
    ipc_response_t resp;
    printf("Sending register request: pid=%d, type=%d, size=%zu\n",
           client->pid, type, sizeof(req));
    if (send_request(client, &req, &resp) != 0)
    {
        perror("send_request");
        close(client->sock_fd);
        return -1;
    }
    if (resp.status != RESP_OK)
    {
        fprintf(stderr, "Registration failed, status=%d\n", resp.status);
        close(client->sock_fd);
        return -1;
    }
    client->registered = true;

    // 获取 IPC 信息
    req.type = REQ_GET_SHM_INFO;
    req.seq = 2;
    printf("Sending register request: pid=%d, type=%d, size=%zu\n",
           client->pid, type, sizeof(req));
    if (send_request(client, &req, &resp) != 0 || resp.status != RESP_OK)
    {
        fprintf(stderr, "Failed to get IPC info\n");
        close(client->sock_fd);
        return -1;
    }

    // client->shm_key = resp.shm_key;
    // client->shm_id = resp.shm_id;
    // client->shm_size = resp.shm_size;
    // client->sem_key = resp.sem_key;
    // client->sem_id = resp.sem_id;

    // 附加共享内存
    client->shm_ptr = shmat(resp.shm_id, NULL, 0666);
    if (client->shm_ptr == (void *)-1)
    {
        perror("shmat");
        close(client->sock_fd);
        return -1;
    }
    client->sem_id = semget(resp.sem_key, 1, 0666); // 直接使用 sem_key 获取 sem_id
    if (client->sem_id < 0)
    {
        perror("semget");
        shmdt(client->shm_ptr);
        close(client->sock_fd);
        return -1;
    }
    printf("IPC client initialized. PID=%d, semid=%d\n",
           client->pid, client->sem_id);

    return 0;
}

void *ipc_client_get_shm(ipc_client_t *client)
{
    return client->shm_ptr;
}

int ipc_client_get_semid(ipc_client_t *client)
{
    return client->sem_id;
}

int ipc_client_heartbeat(ipc_client_t *client)
{
    if (client->type != CLIENT_LONG_TERM)
        return 0;
    ipc_request_t req = {.type = REQ_HEARTBEAT, .pid = client->pid, .seq = 100};
    ipc_response_t resp;
    return send_request(client, &req, &resp);
}

void ipc_client_close(ipc_client_t *client)
{
    if (client->registered)
    {
        ipc_request_t req = {.type = REQ_UNREGISTER, .pid = client->pid, .seq = 200};
        ipc_response_t resp;
        printf("Sending unregister request: pid=%d, type=%d, size=%zu\n",
               client->pid, client->type, sizeof(req));
        send_request(client, &req, &resp);
        printf("Unregister response: status=%d\n", resp.status);
    }
    if (client->shm_ptr)
    {
        shmdt(client->shm_ptr);
        client->shm_ptr = NULL;
    }
    if (client->sock_fd >= 0)
    {
        close(client->sock_fd);
        client->sock_fd = -1;
    }
}