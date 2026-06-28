
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <vector>
#include "producer.h"
#include "producer_device.h"
#include "data_format.h"

using namespace std;

// ======================= 文件作用域信号变量 =======================
// 每个线程拥有独立的信号处理；这些变量在 spectrum_thread 中通过
// sigaction 安装线程专属处理函数后使用
static volatile sig_atomic_t g_prod_shutdown = 0;
static volatile sig_atomic_t g_prod_freq_changed = 0;

// ======================= 内部辅助函数 =======================

/**
 * 原子读取共享内存中的频率和模式 (使用 System V 信号量保护)
 * 成功读取并消耗 data_ready 标志后返回 true，否则 false
 */
static bool read_freq_from_shm(SpectrumData *shm, int sem_id,
                               double *start, double *stop, int *mode)
{
    struct sembuf lock = {0, -1, SEM_UNDO};
    struct sembuf unlock = {0, 1, SEM_UNDO};

    semop(sem_id, &lock, 1);

    if (!shm->data_ready)
    {
        semop(sem_id, &unlock, 1);
        return false;
    }

    *start = shm->start_freq;
    *stop = shm->stop_freq;
    *mode = shm->mode;
    shm->data_ready = 0;

    semop(sem_id, &unlock, 1);
    return true;
}

/**
 * 更新共享内存心跳
 */
static void update_heartbeat(SpectrumData *shm, int sem_id)
{
    struct sembuf lock = {0, -1, SEM_UNDO};
    struct sembuf unlock = {0, 1, SEM_UNDO};

    semop(sem_id, &lock, 1);
    shm->heartbeat = time(NULL);
    semop(sem_id, &unlock, 1);
}

/**
 * 将数据写入文件(含头)，标记为 READY，触发上传者
 *
 * @param mode        0=功率谱, 1=IQ
 * @param start_freq  起始/中心频率 (Hz)
 * @param stop_freq   终止频率 (Hz), IQ 模式传 0
 * @param rbw_hz      分辨率带宽, IQ 模式传 0
 * @param data_type   DATA_TYPE_FLOAT32 或 DATA_TYPE_INT16_INTERLEAVED
 * @param points      数据点数
 * @param data        二进制数据缓冲区
 * @param data_bytes  数据字节数
 * @return 0=成功, -1=失败
 */
static int write_data_file(file_Manager *file_mgr,
                           int mode,
                           double start_freq, double stop_freq,
                           double rbw_hz,
                           int data_type,
                           uint32_t points,
                           const void *data, size_t data_bytes,
                           sem_t *upload_sem)
{
    fileInfo *f = file_mgr->get_file_w();
    if (f == NULL || f->state == OUT)
    {
        fprintf(stderr, "[PRODUCER] No writable file available\n");
        return -1;
    }

    FILE *fp = fopen(f->filename.c_str(), "wb");
    if (!fp)
    {
        perror("fopen");
        return -1;
    }

    // 写入文本头
    if (write_data_header(fp, mode, start_freq, stop_freq,
                          rbw_hz, data_type, points) < 0)
    {
        fclose(fp);
        return -1;
    }

    // 写入二进制数据
    size_t written = fwrite(data, 1, data_bytes, fp);
    fclose(fp);

    if (written != data_bytes)
    {
        fprintf(stderr, "[PRODUCER] fwrite incomplete: %zu/%zu bytes\n",
                written, data_bytes);
        return -1;
    }

    // 标记就绪并唤醒上传者
    f->state = READY;
    sem_post(upload_sem);

    printf("[PRODUCER] Wrote %s: mode=%d, points=%u, bytes=%zu\n",
           f->filename.c_str(), mode, points, data_bytes);
    return 0;
}

// ======================= 模式执行函数 =======================

/**
 * 模式 0: 功率谱采集
 * 使用 SWP_* API，每帧采集一次全扫描并写入文件
 */
static int run_power_spectrum_mode(ProducerArgs *args,
                                   volatile sig_atomic_t *shutdown_flag,
                                   volatile sig_atomic_t *freq_changed)
{
    // 从共享内存读取初始频率
    double start_freq, stop_freq;
    int shm_mode;
    {
        struct sembuf lock = {0, -1, SEM_UNDO};
        struct sembuf unlock = {0, 1, SEM_UNDO};
        semop(args->sem_id, &lock, 1);
        start_freq = args->shm->start_freq;
        stop_freq = args->shm->stop_freq;
        shm_mode = args->shm->mode;
        semop(args->sem_id, &unlock, 1);
    }

    printf("[PS] Initial: %.3f - %.3f Hz\n", start_freq, stop_freq);

    // 打开设备并配置为功率谱模式
    PowerSpectrumState *ps = ps_open_and_config(0, start_freq, stop_freq, 300e3);
    if (!ps)
    {
        fprintf(stderr, "[PS] Failed to open device\n");
        return ERROR;
    }

    uint32_t points = ps_get_points(ps);
    double rbw = ps_get_rbw(ps);

    // 分配数据缓冲区
    vector<double> freq_data(points);
    vector<float> power_data(points);

    int current_mode = MODE_POWER_SPECTRUM;

    printf("[PS] Entered Power Spectrum mode. Points=%u, RBW=%.1f Hz\n",
           points, rbw);

    while (!(*shutdown_flag) && current_mode == MODE_POWER_SPECTRUM)
    {
        // --- 频率/模式更新检查 ---
        if (*freq_changed)
        {
            double new_start, new_stop;
            int new_mode;
            if (read_freq_from_shm(args->shm, args->sem_id,
                                   &new_start, &new_stop, &new_mode))
            {
                printf("[PS] SHM update: %.3f-%.3f Hz, mode=%d\n",
                       new_start, new_stop, new_mode);

                if (new_mode != MODE_POWER_SPECTRUM)
                {
                    // 模式切换：退出当前模式，由上层切换到 IQ
                    current_mode = new_mode;
                    *freq_changed = 0;
                    printf("[PS] Mode switch requested: %d -> %d\n",
                           MODE_POWER_SPECTRUM, new_mode);
                    break;
                }

                // 仅频率变化，重新配置
                if (ps_reconfigure(ps, new_start, new_stop, rbw) == 0)
                {
                    start_freq = new_start;
                    stop_freq = new_stop;
                    points = ps_get_points(ps);
                    freq_data.resize(points);
                    power_data.resize(points);
                }
            }
            *freq_changed = 0;
        }

        // --- 采集一帧功率谱 ---
        if (ps_acquire_sweep(ps, freq_data.data(), power_data.data()) != 0)
        {
            fprintf(stderr, "[PS] Sweep acquisition failed\n");
            break;
        }

        // --- 写入文件 (头 + float32 二进制) ---
        int ret = write_data_file(
            args->file_mgr,
            MODE_POWER_SPECTRUM,
            start_freq, stop_freq,
            rbw,
            DATA_TYPE_FLOAT32,
            points,
            power_data.data(),
            points * sizeof(float),
            args->upload_sem);

        if (ret != 0)
        {
            fprintf(stderr, "[PS] File write failed\n");
            break;
        }

        // --- 心跳 ---
        update_heartbeat(args->shm, args->sem_id);

        // 休眠，避免忙等待
        usleep(100000); // 100ms
    }

    printf("[PS] Exiting Power Spectrum mode...\n");
    ps_close(ps);
    return *shutdown_flag ? ERROR : current_mode;
}

/**
 * 模式 1: IQ 采集
 * 使用 IQS_* API，触发一次采集所有包，拼接后写入文件
 */
static int run_iq_mode(ProducerArgs *args,
                       volatile sig_atomic_t *shutdown_flag,
                       volatile sig_atomic_t *freq_changed)
{
    // 从共享内存读取初始频率 (IQ 模式用 start_freq 作为中心频率)
    double center_freq;
    int shm_mode;
    {
        struct sembuf lock = {0, -1, SEM_UNDO};
        struct sembuf unlock = {0, 1, SEM_UNDO};
        semop(args->sem_id, &lock, 1);
        center_freq = args->shm->start_freq; // IQ 模式: start_freq == 中心频率
        shm_mode = args->shm->mode;
        semop(args->sem_id, &unlock, 1);
    }

    printf("[IQ] Initial center: %.3f Hz\n", center_freq);

    // 打开设备并配置为 IQ 模式
    IQState *iq = iq_open_and_config(0, center_freq,
                                     0.0,    // RefLevel_dBm
                                     2,      // DecimateFactor
                                     16242); // TriggerLength
    if (!iq)
    {
        fprintf(stderr, "[IQ] Failed to open device\n");
        return ERROR;
    }

    uint32_t samples = iq_get_samples(iq);
    uint32_t total_ints = iq_get_total_ints(iq);

    // 分配 IQ 数据缓冲区 (I/Q 交织的 int16)
    vector<int16_t> iq_data(total_ints);

    int current_mode = MODE_IQ;
    double ref_level_dbm = 0.0;
    int decimate = 2;
    uint32_t trig_len = 16242;

    printf("[IQ] Entered IQ mode. Center=%.3f Hz, Samples=%u, Ints=%u\n",
           center_freq, samples, total_ints);

    while (!(*shutdown_flag) && current_mode == MODE_IQ)
    {
        // --- 频率/模式更新检查 ---
        if (*freq_changed)
        {
            double new_center, new_stop_unused;
            int new_mode;
            if (read_freq_from_shm(args->shm, args->sem_id,
                                   &new_center, &new_stop_unused, &new_mode))
            {
                printf("[IQ] SHM update: center=%.3f Hz, mode=%d\n",
                       new_center, new_mode);

                if (new_mode != MODE_IQ)
                {
                    current_mode = new_mode;
                    *freq_changed = 0;
                    printf("[IQ] Mode switch requested: %d -> %d\n",
                           MODE_IQ, new_mode);
                    break;
                }

                // 仅频率变化，重新配置
                if (iq_reconfigure(iq, new_center, ref_level_dbm,
                                   decimate, trig_len) == 0)
                {
                    center_freq = new_center;
                    samples = iq_get_samples(iq);
                    total_ints = iq_get_total_ints(iq);
                    iq_data.resize(total_ints);
                }
            }
            *freq_changed = 0;
        }

        // --- 采集一帧 IQ 数据 ---
        if (iq_acquire_stream(iq, iq_data.data()) != 0)
        {
            fprintf(stderr, "[IQ] Stream acquisition failed\n");
            break;
        }

        // --- 写入文件 (头 + int16 交织二进制, IQ 模式 rbw=0) ---
        int ret = write_data_file(
            args->file_mgr,
            MODE_IQ,
            center_freq, 0.0, // IQ: start=center, stop=0
            0.0,              // rbw=0 (不适用)
            DATA_TYPE_INT16_INTERLEAVED,
            samples,
            iq_data.data(),
            total_ints * sizeof(int16_t),
            args->upload_sem);

        if (ret != 0)
        {
            fprintf(stderr, "[IQ] File write failed\n");
            break;
        }

        // --- 心跳 ---
        update_heartbeat(args->shm, args->sem_id);

        usleep(500000); // 500ms
    }

    printf("[IQ] Exiting IQ mode...\n");
    iq_close(iq);
    return *shutdown_flag ? ERROR : current_mode;
}

// ======================= 信号处理 =======================

static void prod_signal_handler(int sig)
{
    if (sig == SIGUSR1)
    {
        g_prod_freq_changed = 1;
    }
    else if (sig == SIGINT || sig == SIGTERM)
    {
        g_prod_shutdown = 1;
    }
}

// ======================= 生产者线程入口 =======================

void *spectrum_thread(void *arg)
{
    ProducerArgs *args = (ProducerArgs *)arg;

    // 安装本线程的信号处理
    struct sigaction sa;
    sa.sa_handler = prod_signal_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    // 初始化共享内存中的频谱仪 PID 和心跳
    {
        struct sembuf lock = {0, -1, SEM_UNDO};
        struct sembuf unlock = {0, 1, SEM_UNDO};
        semop(args->sem_id, &lock, 1);
        args->shm->spectrum_pid = getpid();
        args->shm->heartbeat = time(NULL);
        args->shm->shutdown = 0;
        semop(args->sem_id, &unlock, 1);
    }

    printf("[SPECTRUM] Thread started, PID=%d\n", getpid());

    int current_mode = MODE_POWER_SPECTRUM; // 默认功率谱

    while (!atomic_load(args->shutdown))
    {
        switch (current_mode)
        {
        case MODE_POWER_SPECTRUM:
            printf("[SPECTRUM] → Power Spectrum mode\n");
            current_mode = run_power_spectrum_mode(
                args, &g_prod_shutdown, &g_prod_freq_changed);
            break;

        case MODE_IQ:
            printf("[SPECTRUM] → IQ mode\n");
            current_mode = run_iq_mode(
                args, &g_prod_shutdown, &g_prod_freq_changed);
            break;

        default:
            fprintf(stderr, "[SPECTRUM] Unknown mode %d, exiting\n", current_mode);
            atomic_store(args->shutdown, 1);
            break;
        }

        if (current_mode == ERROR)
        {
            fprintf(stderr, "[SPECTRUM] Mode returned ERROR, shutting down\n");
            atomic_store(args->shutdown, 1);
            break;
        }
    }

    // 通知其他进程即将退出
    {
        struct sembuf lock = {0, -1, SEM_UNDO};
        struct sembuf unlock = {0, 1, SEM_UNDO};
        semop(args->sem_id, &lock, 1);
        args->shm->shutdown = 1;
        semop(args->sem_id, &unlock, 1);
    }

    // 唤醒可能阻塞的上传线程
    sem_post(args->upload_sem);

    printf("[SPECTRUM] Thread exiting.\n");
    return NULL;
}