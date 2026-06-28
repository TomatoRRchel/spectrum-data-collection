
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

// 线程级信号标志
static volatile sig_atomic_t g_prod_shutdown = 0;
static volatile sig_atomic_t g_prod_freq_changed = 0;

// 读取共享内存中的频率/模式 (消耗 data_ready)
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

// 更新共享内存心跳
static void update_heartbeat(SpectrumData *shm, int sem_id)
{
    struct sembuf lock = {0, -1, SEM_UNDO};
    struct sembuf unlock = {0, 1, SEM_UNDO};
    semop(sem_id, &lock, 1);
    shm->heartbeat = time(NULL);
    semop(sem_id, &unlock, 1);
}

// 写入文件 (头 + 二进制) 并通知上传者
static int write_data_file(file_Manager *file_mgr,
                           int mode, double start_freq, double stop_freq,
                           double rbw_hz, int data_type, uint32_t points,
                           const void *data, size_t data_bytes, sem_t *upload_sem)
{
    fileInfo *f = file_mgr->get_file_w();
    if (!f || f->state == OUT)
    {
        fprintf(stderr, "[PROD] No writable file\n");
        return -1;
    }
    FILE *fp = fopen(f->filename.c_str(), "wb");
    if (!fp)
    {
        perror("fopen");
        return -1;
    }
    if (write_data_header(fp, mode, start_freq, stop_freq, rbw_hz, data_type, points) < 0)
    {
        fclose(fp);
        return -1;
    }
    size_t wr = fwrite(data, 1, data_bytes, fp);
    fclose(fp);
    if (wr != data_bytes)
    {
        fprintf(stderr, "[PROD] fwrite short: %zu/%zu\n", wr, data_bytes);
        return -1;
    }
    f->state = READY;
    sem_post(upload_sem);
    printf("[PROD] %s mode=%d pts=%u %zuB\n", f->filename.c_str(), mode, points, data_bytes);
    return 0;
}

// ======================== 功率谱模式 ========================
static int run_power_spectrum_mode(ProducerArgs *args,
                                   volatile sig_atomic_t *sd, volatile sig_atomic_t *fc)
{
    double start_freq, stop_freq;
    int shm_mode;
    {
        struct sembuf lk = {0, -1, SEM_UNDO}, ul = {0, 1, SEM_UNDO};
        semop(args->sem_id, &lk, 1);
        start_freq = args->shm->start_freq;
        stop_freq = args->shm->stop_freq;
        shm_mode = args->shm->mode;
        semop(args->sem_id, &ul, 1);
    }
    printf("[PS] Init %.3f-%.3f Hz\n", start_freq, stop_freq);

    PowerSpectrumState *ps = ps_open_and_config(0, start_freq, stop_freq, 300e3);
    if (!ps)
    {
        fprintf(stderr, "[PS] Open failed\n");
        return ERROR;
    }

    uint32_t pts = ps_get_points(ps);
    double rbw = ps_get_rbw(ps);
    vector<double> freq(pts);
    vector<float> pwr(pts);
    int cur = MODE_POWER_SPECTRUM;
    printf("[PS] Running, pts=%u rbw=%.1f\n", pts, rbw);

    while (!*sd && cur == MODE_POWER_SPECTRUM)
    {
        if (*fc)
        {
            double ns, ne;
            int nm;
            if (read_freq_from_shm(args->shm, args->sem_id, &ns, &ne, &nm))
            {
                printf("[PS] SHM: %.3f-%.3f mode=%d\n", ns, ne, nm);
                if (nm != MODE_POWER_SPECTRUM)
                {
                    cur = nm;
                    *fc = 0;
                    break;
                }
                if (!ps_reconfigure(ps, ns, ne, rbw))
                {
                    start_freq = ns;
                    stop_freq = ne;
                    pts = ps_get_points(ps);
                    freq.resize(pts);
                    pwr.resize(pts);
                }
            }
            *fc = 0;
        }
        if (ps_acquire_sweep(ps, freq.data(), pwr.data()))
        {
            fprintf(stderr, "[PS] Acq fail\n");
            break;
        }
        if (write_data_file(args->file_mgr, MODE_POWER_SPECTRUM, start_freq, stop_freq,
                            rbw, DATA_TYPE_FLOAT32, pts, pwr.data(), pts * sizeof(float),
                            args->upload_sem))
        {
            fprintf(stderr, "[PS] Write fail\n");
            break;
        }
        update_heartbeat(args->shm, args->sem_id);
        usleep(100000);
    }
    printf("[PS] Exit\n");
    ps_close(ps);
    return *sd ? ERROR : cur;
}

// ======================== IQ 模式 ========================
static int run_iq_mode(ProducerArgs *args,
                       volatile sig_atomic_t *sd, volatile sig_atomic_t *fc)
{
    double center;
    int shm_mode;
    {
        struct sembuf lk = {0, -1, SEM_UNDO}, ul = {0, 1, SEM_UNDO};
        semop(args->sem_id, &lk, 1);
        center = args->shm->start_freq; // IQ: start_freq = 中心频率
        shm_mode = args->shm->mode;
        semop(args->sem_id, &ul, 1);
    }
    printf("[IQ] Init center=%.3f Hz\n", center);

    IQState *iq = iq_open_and_config(0, center, 0.0, 2, 16242);
    if (!iq)
    {
        fprintf(stderr, "[IQ] Open failed\n");
        return ERROR;
    }

    uint32_t samples = iq_get_samples(iq);
    uint32_t total = iq_get_total_ints(iq);
    vector<int16_t> buf(total);
    int cur = MODE_IQ;
    double rl = 0.0;
    int dc = 2;
    uint32_t tl = 16242;
    printf("[IQ] Running, samples=%u\n", samples);

    while (!*sd && cur == MODE_IQ)
    {
        if (*fc)
        {
            double nc, nu;
            int nm;
            if (read_freq_from_shm(args->shm, args->sem_id, &nc, &nu, &nm))
            {
                printf("[IQ] SHM: center=%.3f mode=%d\n", nc, nm);
                if (nm != MODE_IQ)
                {
                    cur = nm;
                    *fc = 0;
                    break;
                }
                if (!iq_reconfigure(iq, nc, rl, dc, tl))
                {
                    center = nc;
                    samples = iq_get_samples(iq);
                    total = iq_get_total_ints(iq);
                    buf.resize(total);
                }
            }
            *fc = 0;
        }
        if (iq_acquire_stream(iq, buf.data()))
        {
            fprintf(stderr, "[IQ] Acq fail\n");
            break;
        }
        if (write_data_file(args->file_mgr, MODE_IQ, center, 0.0, 0.0,
                            DATA_TYPE_INT16_INTERLEAVED, samples,
                            buf.data(), total * sizeof(int16_t),
                            args->upload_sem))
        {
            fprintf(stderr, "[IQ] Write fail\n");
            break;
        }
        update_heartbeat(args->shm, args->sem_id);
        usleep(500000);
    }
    printf("[IQ] Exit\n");
    iq_close(iq);
    return *sd ? ERROR : cur;
}

// ======================== 信号处理 ========================
static void prod_signal_handler(int sig)
{
    if (sig == SIGUSR1)
        g_prod_freq_changed = 1;
    else if (sig == SIGINT || sig == SIGTERM)
        g_prod_shutdown = 1;
}

// ======================== 线程入口 ========================
void *spectrum_thread(void *arg)
{
    ProducerArgs *a = (ProducerArgs *)arg;
    // struct sigaction sa = {.sa_handler = prod_signal_handler, .sa_flags = 0};
    struct sigaction sa;
    sa.sa_handler = prod_signal_handler;
    sigemptyset(&sa.sa_mask); // 清空信号掩码
    sa.sa_flags = 0;

    sigemptyset(&sa.sa_mask);
    sigaction(SIGUSR1, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGTERM, &sa, NULL);

    {
        struct sembuf lk = {0, -1, SEM_UNDO}, ul = {0, 1, SEM_UNDO};
        semop(a->sem_id, &lk, 1);
        a->shm->spectrum_pid = getpid();
        a->shm->heartbeat = time(NULL);
        a->shm->shutdown = 0;
        semop(a->sem_id, &ul, 1);
    }
    printf("[SPECTRUM] Started PID=%d\n", getpid());

    int mode = MODE_POWER_SPECTRUM;
    while (!atomic_load(a->shutdown))
    {
        switch (mode)
        {
        case MODE_POWER_SPECTRUM:
            mode = run_power_spectrum_mode(a, &g_prod_shutdown, &g_prod_freq_changed);
            break;
        case MODE_IQ:
            mode = run_iq_mode(a, &g_prod_shutdown, &g_prod_freq_changed);
            break;
        default:
            fprintf(stderr, "[SPECTRUM] Unknown mode %d\n", mode);
            atomic_store(a->shutdown, 1);
            break;
        }
        if (mode == ERROR)
        {
            atomic_store(a->shutdown, 1);
            break;
        }
    }

    {
        struct sembuf lk = {0, -1, SEM_UNDO}, ul = {0, 1, SEM_UNDO};
        semop(a->sem_id, &lk, 1);
        a->shm->shutdown = 1;
        semop(a->sem_id, &ul, 1);
    }
    sem_post(a->upload_sem);
    printf("[SPECTRUM] Exit\n");
    return NULL;
}