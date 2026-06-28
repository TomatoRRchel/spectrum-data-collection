/*
 * cli_control.c — 频谱仪 CLI 控制器
 *
 * 大模型/外部程序调用该二进制来控制频谱仪模式和频率。
 * 用法: cli_control <mode> <start_freq> <stop_freq>
 *   mode:       0=功率谱, 1=IQ
 *   start_freq: 起始频率 (Hz), IQ 模式即中心频率
 *   stop_freq:  终止频率 (Hz), IQ 模式传 0
 *
 * 示例:
 *   cli_control 0 13000000 18000000   # 功率谱 13-18MHz
 *   cli_control 1 1000000000 0        # IQ 中心 1GHz
 *
 * 流程:
 *   连接守护进程 → 获取共享内存 → 写入频率/模式 → 通知生产者 → 注销退出
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "ipc_client.h" // 复用现有客户端库
#include "shm_ipc.h"    // SpectrumData 定义

#define WAIT_SPECTRUM_TIMEOUT_S 10
#define MODE_POWER_SPECTRUM 0
#define MODE_IQ 1

static void print_usage(const char *prog)
{
    fprintf(stderr,
            "用法: %s <mode> <start_freq> <stop_freq>\n"
            "  mode:       0=功率谱, 1=IQ\n"
            "  start_freq: 起始频率 (Hz), IQ模式为中心频率\n"
            "  stop_freq:  终止频率 (Hz), IQ模式传0\n"
            "\n"
            "示例:\n"
            "  %s 0 13000000 18000000    # 功率谱 13-18MHz\n"
            "  %s 1 1000000000 0         # IQ 中心 1GHz\n",
            prog, prog, prog);
}

int main(int argc, char *argv[])
{
    if (argc != 4)
    {
        print_usage(argv[0]);
        return 1;
    }

    int mode = atoi(argv[1]);
    double start_freq = atof(argv[2]);
    double stop_freq = atof(argv[3]);

    if (mode != MODE_POWER_SPECTRUM && mode != MODE_IQ)
    {
        fprintf(stderr, "错误: mode 必须为 0(功率谱) 或 1(IQ)\n");
        return 1;
    }
    if (start_freq <= 0)
    {
        fprintf(stderr, "错误: 频率必须 > 0\n");
        return 1;
    }
    if (mode == MODE_POWER_SPECTRUM && start_freq >= stop_freq)
    {
        fprintf(stderr, "错误: 功率谱模式下起始频率必须 < 终止频率\n");
        return 1;
    }

    printf("[cli] 连接守护进程...\n");

    // 1. 连接守护进程并注册为临时客户端
    ipc_client_t client;
    if (ipc_client_init(&client, CLIENT_TRANSIENT) != 0)
    {
        fprintf(stderr, "错误: 无法连接/注册到守护进程\n");
        return 2;
    }

    SpectrumData *shm = (SpectrumData *)client.shm_ptr;
    int sem_id = client.sem_id;

    // 2. 等待频谱仪生产者就绪 (spectrum_pid != 0)
    printf("[cli] 等待频谱仪就绪...\n");
    int waited = 0;
    while (waited < WAIT_SPECTRUM_TIMEOUT_S)
    {
        struct sembuf lock = {0, -1, SEM_UNDO};
        struct sembuf unlock = {0, 1, SEM_UNDO};
        semop(sem_id, &lock, 1);
        pid_t spid = shm->spectrum_pid;
        semop(sem_id, &unlock, 1);

        if (spid != 0)
        {
            printf("[cli] 频谱仪 PID=%d 就绪\n", spid);
            break;
        }
        sleep(1);
        waited++;
    }

    if (waited >= WAIT_SPECTRUM_TIMEOUT_S)
    {
        fprintf(stderr, "错误: 等待频谱仪超时 (%ds)\n", WAIT_SPECTRUM_TIMEOUT_S);
        ipc_client_close(&client);
        return 3;
    }

    pid_t spectrum_pid;
    {
        struct sembuf lock = {0, -1, SEM_UNDO};
        struct sembuf unlock = {0, 1, SEM_UNDO};
        semop(sem_id, &lock, 1);
        spectrum_pid = shm->spectrum_pid;
        semop(sem_id, &unlock, 1);
    }

    // 3. 写入频率和模式到共享内存
    printf("[cli] 设置: mode=%d, freq=%.0f-%.0f Hz\n",
           mode, start_freq, stop_freq);

    {
        struct sembuf lock = {0, -1, SEM_UNDO};
        struct sembuf unlock = {0, 1, SEM_UNDO};
        semop(sem_id, &lock, 1);

        shm->start_freq = start_freq;
        shm->stop_freq = stop_freq;
        shm->mode = mode;
        shm->data_ready = 1;
        shm->timestamp = time(NULL);

        semop(sem_id, &unlock, 1);
    }

    // 4. 通知频谱仪生产者
    if (kill(spectrum_pid, SIGUSR1) != 0)
    {
        fprintf(stderr, "警告: 通知频谱仪失败 (PID=%d): %s\n",
                spectrum_pid, strerror(errno));
    }
    else
    {
        printf("[cli] 已发送 SIGUSR1 到 PID=%d\n", spectrum_pid);
    }

    // 5. 注销并退出
    ipc_client_close(&client);
    printf("[cli] 完成\n");
    return 0;
}
