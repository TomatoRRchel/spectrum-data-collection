#include "shm_ipc.h"

// 信号量操作结构
static struct sembuf lock_op = {0, -1, SEM_UNDO};
static struct sembuf unlock_op = {0, 1, SEM_UNDO};

// static int init_ipc_resources(void)
// {
//     bool shm_is_new = false;
//     bool sem_is_new = false;
//     int shm_id = -1;
//     int sem_id = -1;
//     SpectrumData *shm_ptr = NULL;
//     bool attached = false;
//     bool ref_inc_done = false;

//     // 1. 初始化共享内存
//     shm_id = init_shared_memory(&shm_is_new);
//     if (shm_id < 0)
//     {
//         fprintf(stderr, "Failed to init shared memory\n");
//         goto cleanup_fail;
//     }

//     // 2. 初始化信号量
//     sem_id = init_semaphore(&sem_is_new);
//     if (sem_id < 0)
//     {
//         fprintf(stderr, "Failed to init semaphore\n");
//         goto cleanup_fail;
//     }

//     // 3. 附加共享内存
//     shm_ptr = (SpectrumData *)attach_shared_memory();
//     if (!shm_ptr)
//     {
//         fprintf(stderr, "Failed to attach shared memory\n");
//         goto cleanup_fail;
//     }
//     attached = true;

//     // 4. 处理共享内存内容初始化或引用计数增加
//     if (shm_is_new && sem_is_new)
//     {
//         // 两者全新：初始化内容，引用计数设为 1
//         memset(shm_ptr, 0, sizeof(SpectrumData));
//         shm_ptr->ref_count = 1;
//         shm_ptr->start_freq = 15e6;
//         shm_ptr->stop_freq = 16e6;
//         shm_ptr->data_ready = 0;
//         shm_ptr->shutdown = 0;
//         shm_ptr->mode = MODE_POWER_SPECTRUM;
//         shm_ptr->producer_pid = getpid();
//         shm_ptr->heartbeat = time(NULL);
//         printf("IPC resources created and initialized. ref_count=1\n");
//     }
//     else
//     {
//         ipc_ref_inc(shm_ptr, sem_id);
//         ref_inc_done = true;
//         // 可选更新动态字段
//         shm_ptr->producer_pid = getpid();
//         printf("Attached to existing IPC resources. ref_count incremented.\n");
//     }

//     // 5. 初始化本地上传信号量（POSIX）
//     if (sem_init(&g_upload_sem, 0, 0) != 0)
//     {
//         perror("sem_init upload_sem");
//         goto cleanup_fail;
//     }

//     // 全部成功，保存全局变量
//     g_shm_id = shm_id;
//     g_sem_id = sem_id;
//     g_shm_data = shm_ptr;
//     return 0;

// cleanup_fail:
//     // 回滚操作（逆序）
//     if (ref_inc_done)
//     {
//         // 如果已经增加了引用计数，需要减回去
//         ipc_ref_dec(shm_ptr, sem_id);
//     }
//     if (attached)
//     {
//         detach_shared_memory(shm_ptr);
//     }
//     // 注意：只有当前进程创建的资源才需要销毁
//     // 由于其他进程可能正在使用，不能随意删除已存在的资源
//     if (shm_is_new && shm_id >= 0)
//     {
//         // 只有我们自己新创建的共享内存，且尚未被其他进程附加，才可安全删除
//         destroy_shared_memory_by_id(shm_id);
//     }
//     if (sem_is_new && sem_id >= 0)
//     {
//         destroy_semaphore_by_id(sem_id);
//     }
//     // POSIX 信号量未成功初始化，无需销毁
//     return -1;
// }

/**
 * 初始化或获取共享内存
 * @param out_is_new 输出参数，若为新创建则置为 1，否则为 0（可为 NULL）
 * @return 成功返回 shmid，失败返回 -1
 */
int init_shared_memory(bool *out_is_new)
{
    bool shm_id;
    bool is_new = 0;
    printf("DEBUG: SHM_KEY = 0x%x (%d), size = %zu\n", SHM_KEY, SHM_KEY, sizeof(SpectrumData));
    // 先尝试不带 IPC_CREAT 获取，检查是否已存在
    shm_id = shmget(SHM_KEY, sizeof(SpectrumData), 0666);
    if (shm_id >= 0)
    {
        // 共享内存已存在
        is_new = 0;
        printf("Shared memory already exists with ID: %d\n", shm_id);
    }
    else if (errno == ENOENT)
    {
        // 不存在，则创建
        shm_id = shmget(SHM_KEY, sizeof(SpectrumData), IPC_CREAT | IPC_EXCL | 0666);
        if (shm_id < 0)
        {
            perror("shmget create failed");
            return -1;
        }
        is_new = 1;
        printf("Shared memory created (new) with ID: %d\n", shm_id);
    }
    else
    {
        // 其他错误（如权限问题）
        perror("shmget failed");
        return -2;
    }

    if (out_is_new)
    {
        *out_is_new = is_new;
    }
    return shm_id;
}

void ipc_ref_inc(SpectrumData *shm, int sem_id)
{
    struct sembuf lock = {0, -1, SEM_UNDO};
    struct sembuf unlock = {0, 1, SEM_UNDO};
    semop(sem_id, &lock, 1);
    shm->ref_count++;
    semop(sem_id, &unlock, 1);
    printf("IPC ref_count incremented to %d\n", shm->ref_count);
}

// 减少引用计数，返回减后的值；若为0则返回0，调用者可据此清理
int ipc_ref_dec(SpectrumData *shm, int sem_id)
{
    struct sembuf lock = {0, -1, SEM_UNDO};
    struct sembuf unlock = {0, 1, SEM_UNDO};
    semop(sem_id, &lock, 1);
    int remaining = --shm->ref_count;
    semop(sem_id, &unlock, 1);
    printf("IPC ref_count decremented to %d\n", remaining);
    return remaining;
}

// 附加共享内存
void *attach_shared_memory(void)
{
    int shm_id = shmget(SHM_KEY, 0, 0666);
    if (shm_id < 0)
    {
        perror("shmget (attach) failed");
        return NULL;
    }

    void *shm_ptr = shmat(shm_id, NULL, 0);
    if (shm_ptr == (void *)-1)
    {
        perror("shmat failed");
        return NULL;
    }

    return shm_ptr;
}

// 分离共享内存
int detach_shared_memory(void *shm_ptr)
{
    if (shmdt(shm_ptr) == -1)
    {
        perror("shmdt failed");
        return -1;
    }
    return 0;
}

// 销毁共享内存
int destroy_shared_memory(void)
{
    int shm_id = shmget(SHM_KEY, 0, 0666);
    if (shm_id < 0)
    {
        // 共享内存可能已被删除
        return 0;
    }

    if (shmctl(shm_id, IPC_RMID, NULL) == -1)
    {
        perror("shmctl IPC_RMID failed");
        return -1;
    }

    printf("Shared memory destroyed\n");
    return 0;
}

// 初始化信号量
int init_semaphore(bool *out_is_new)
{
    bool is_new = false;
    int sem_id = semget(SEM_KEY, 1, 0666);
    if (sem_id >= 0)
    {
        printf("Semaphore already exists with ID: %d\n", sem_id);
        if (out_is_new)
        {
            *out_is_new = is_new;
        }
        return sem_id;
    }
    else if (errno == ENOENT)
    {
        sem_id = semget(SEM_KEY, 1, IPC_CREAT | IPC_EXCL | 0666);
        if (sem_id < 0)
        {
            perror("semget create failed");
            return -1;
        }
        printf("Semaphore created (new) with ID: %d\n", sem_id);
        union semun
        {
            int val;
            struct semid_ds *buf;
            unsigned short *array;
        } arg;

        arg.val = 1;
        if (semctl(sem_id, 0, SETVAL, arg) == -1)
        {
            perror("semctl SETVAL failed");
            return -1;
        }
        is_new = true;
        if (out_is_new)
        {
            *out_is_new = is_new;
        }
        printf("Semaphore created with ID: %d\n", sem_id);
        return sem_id;
    }
    else
    {
        perror("semget failed");
        return -1;
    }
}

// 锁定信号量
int lock_semaphore(int sem_id)
{
    if (semop(sem_id, &lock_op, 1) == -1)
    {
        perror("semop lock failed");
        return -1;
    }
    return 0;
}

// 解锁信号量
int unlock_semaphore(int sem_id)
{
    if (semop(sem_id, &unlock_op, 1) == -1)
    {
        perror("semop unlock failed");
        return -1;
    }
    return 0;
}

// 销毁信号量
int destroy_semaphore(int sem_id)
{
    if (semctl(sem_id, 0, IPC_RMID) == -1)
    {
        perror("semctl IPC_RMID failed");
        return -1;
    }

    printf("Semaphore destroyed\n");
    return 0;
}
