
#include "w_thread.h"
#include "shm_ipc.h"

#define DATA_POINTS 2048

sem_t up_sem;
pthread_mutex_t mut_arr[NUM];
file_Manager fileManager;
double w_time;
bool data_ready = 0;
bool shutdown_requested = 0;
// bool first_run = false;
double g_current_start_freq = 13e6;
double g_current_stop_freq = 18e6;

int shmid = -1;
int semid = -1;
SpectrumData *shared_data = nullptr;

void semaphore_lock()
{
    struct sembuf sb;
    sb.sem_num = 0;
    sb.sem_op = -1;
    sb.sem_flg = SEM_UNDO;

    if (semop(semid, &sb, 1) == -1)
    {
        perror("semop lock failed");
    }
}

void semaphore_unlock()
{
    struct sembuf sb;
    sb.sem_num = 0;
    sb.sem_op = 1;
    sb.sem_flg = SEM_UNDO;

    if (semop(semid, &sb, 1) == -1)
    {
        perror("semop unlock failed");
    }
}

void signal_handler(int sig)
{
#ifdef DE_BUG
    printf("in the sig process\n");
#endif
    if (sig == SIGUSR1)
    {
        data_ready = 1;
        g_freq_changed = true;
    }
    else if (sig == SIGUSR2)
    {
        shutdown_requested = 1;
    }
}

bool init_shared_memory_0()
{
    // 使用与consumer.c相同的键值
    shmid = shmget(SHM_KEY, sizeof(SpectrumData), 0666);
    if (shmid == -1)
    {
        shmid = shmget(SHM_KEY, sizeof(SpectrumData), 0666 | IPC_CREAT);
        if (shmid == -1)
        {
            perror("share memory create failed:");
            return false;
        }

        shared_data = (SpectrumData *)shmat(shmid, NULL, 0);
        if (shared_data == (void *)-1)
        {
            perror("share init failed:");
            return false;
        }

        // 初始化SpectrumData结构
        shared_data->start_freq = g_current_start_freq;
        shared_data->stop_freq = g_current_stop_freq;
        shared_data->producer_pid = getpid();
        shared_data->consumer_pid = 0;
        shared_data->sequence = 0;
        shared_data->timestamp = time(NULL);
        shared_data->data_ready = 0;
        shared_data->shutdown = 0;
    }
    else
    {
        shared_data = (SpectrumData *)shmat(shmid, NULL, 0);
        if (shared_data == (void *)-1)
        {
            perror("add share memory failed:");
            return false;
        }
    }

    semid = semget(SEM_KEY, 1, 0666);
    if (semid == -1)
    {
        semid = semget(SEM_KEY, 1, 0666 | IPC_CREAT);
        if (semid == -1)
        {
            perror("P sem create failed:");
            shmdt(shared_data);
            return false;
        }

        union semun
        {
            int val;
            struct semid_ds *buf;
            unsigned short *array;
        } arg;

        arg.val = 1;
        if (semctl(semid, 0, SETVAL, arg) == -1)
        {
            perror("P sem init failed:");
            shmdt(shared_data);
            return false;
        }
    }

#ifdef DE_BUG
    printf("mamp and sem init over\n");
#endif
    return true;
}

void cleanup_shared_memory()
{
    if (shared_data != nullptr && shared_data != (void *)-1)
    {
        shmdt(shared_data);
        shared_data = nullptr;
    }
}

bool read_frequency_from_shared_memory(double *start_freq, double *stop_freq)
{
    if (shared_data == nullptr || shared_data == (void *)-1)
    {
        return false;
    }

    semaphore_lock();

    // 检查关闭标志
    if (shared_data->shutdown)
    {
        printf("Writer: Shutdown flag detected\n");
        semaphore_unlock();
        return false; // 关闭信号
    }

    // 读取数据
    if (shared_data->data_ready)
    {
        *start_freq = shared_data->start_freq;
        *stop_freq = shared_data->stop_freq;

        // 清除就绪标志
        shared_data->data_ready = 0;

        semaphore_unlock();
        return true;
    }

    semaphore_unlock();
    return false; // 无新数据
}
void init_sync_mechanisms()
{
    for (int i = 0; i < 3; i++)
    {
        pthread_mutex_init(&mut_arr[i], NULL);
    }
}

void *write_thread(void *arg)
{
    int Status = 0;
    void *Device = NULL;
    int DevNum = 0;
    int fileCounter = 0;
    fileInfo *curr_file = nullptr;
    init_sync_mechanisms();
    fileManager.look();
    /*   init_sync_mechanisms();
       if (!init_shared_memory())
       {
           printf("share memory init failed\n");
           sem_destroy(&up_sem);
           cleanup_shared_memory();
           return NULL;
       }*/
    sem_init(&up_sem, 0, 0);

    // 设置信号处理
    signal(SIGUSR1, signal_handler);
    signal(SIGUSR2, signal_handler);

    BootProfile_TypeDef BootProfile;
    BootInfo_TypeDef BootInfo;

    BootProfile.DevicePowerSupply = USBPortAndPowerPort;

    BootProfile.PhysicalInterface = USB;
    Status = Device_Open(&Device, DevNum, &BootProfile, &BootInfo);
    Device_Open_ErrorHandling(Status, &Device, DevNum, &BootProfile, &BootInfo);

    SWP_Profile_TypeDef SWP_ProfileIn;
    SWP_Profile_TypeDef SWP_ProfileOut;
    SWP_TraceInfo_TypeDef TraceInfo;

    SWP_ProfileDeInit(&Device, &SWP_ProfileIn);

    SWP_ProfileIn.StartFreq_Hz = g_current_start_freq;
    SWP_ProfileIn.StopFreq_Hz = g_current_stop_freq;
    SWP_ProfileIn.RBW_Hz = 300e3;

    Status = SWP_Configuration(&Device, &SWP_ProfileIn, &SWP_ProfileOut, &TraceInfo);
    SWP_Configuration_ErrorHandling(Status, &Device, DevNum, &BootProfile, &BootInfo, &SWP_ProfileIn, &SWP_ProfileOut, &TraceInfo);

    vector<double> Frequency(TraceInfo.FullsweepTracePoints);
    vector<float> PowerSpec_dBm(TraceInfo.FullsweepTracePoints);
    int HopIndex = 0;
    int FrameIndex = 0;
    MeasAuxInfo_TypeDef MeasAuxInfo;
#ifdef DE_BUG
    printf("PID: %d\n", getpid());
    printf("initial frequency: %.1f Hz - %.1f Hz\n",
           g_current_start_freq, g_current_stop_freq);
#endif
    fileInfo file_temp;
    FILE *fp = NULL;
    struct timespec start, end;
    char count = 0;
    int _index;

    // 注册生产者PID
    if (init_shared_memory_0())
    {
        semaphore_lock();
        shared_data->consumer_pid = getpid();
        semaphore_unlock();
    }

    printf("\nWriter thread: Waiting for frequency updates...\n");
    printf("Press Ctrl+C to exit\n\n");

    while (!shutdown_requested)
    {
        w_time = 0;
        clock_gettime(CLOCK_MONOTONIC, &start);

        if (data_ready || g_freq_changed)
        {
#ifdef DE_BUG
            printf("start change...\n");
#endif
            double new_start_freq, new_stop_freq;
            if (read_frequency_from_shared_memory(&new_start_freq, &new_stop_freq))
            {
                SWP_ProfileDeInit(&Device, &SWP_ProfileIn);
                SWP_ProfileIn.StartFreq_Hz = new_start_freq;
                SWP_ProfileIn.StopFreq_Hz = new_stop_freq;
                SWP_ProfileIn.RBW_Hz = 300e3;

                Status = SWP_Configuration(&Device, &SWP_ProfileIn, &SWP_ProfileOut, &TraceInfo);
                if (Status != APIRETVAL_NoError)
                {
                    SWP_Configuration_ErrorHandling(Status, &Device, DevNum, &BootProfile, &BootInfo, &SWP_ProfileIn, &SWP_ProfileOut, &TraceInfo);
                    break;
                }
                else
                {
#ifdef DE_BUG
                    printf("get new frequency: %.1f Hz - %.1f Hz\n",
                           new_start_freq, new_stop_freq);
#endif
                    g_current_start_freq = new_start_freq;
                    g_current_stop_freq = new_stop_freq;
                    first_run = true;
                }
            }
            else
            {
                printf("get data failed\n");
                break;
            }

            g_freq_changed = false;
            first_run = true;
        }

        int HopIndex = 0;
        int FrameIndex = 0;
#ifdef DE_BUG
        printf("start collet\n");
#endif
        for (int i = 0; i < TraceInfo.TotalHops; i++)
        {
            Status = SWP_GetPartialSweep(&Device, Frequency.data() + i * TraceInfo.PartialsweepTracePoints, PowerSpec_dBm.data() + i * TraceInfo.PartialsweepTracePoints, &HopIndex, &FrameIndex, &MeasAuxInfo); // 获取频谱数据。

            if (Status != APIRETVAL_NoError)
            {
                SWP_ErrorHandlingExceptOpenAndConfiguration(Status, &Device, DevNum, &BootProfile, &BootInfo, &SWP_ProfileIn, &SWP_ProfileOut, &TraceInfo);
            }
        }

        if (first_run)
        {
            fp = fopen("./fre.txt", "w");
            if (fp != NULL)
            {
                for (size_t i = 0; i < Frequency.size(); i++)
                {
                    fprintf(fp, "%.6f\n", Frequency[i]);
                }
                fclose(fp);
#ifdef DE_BUG
                printf("first frequency data \n");
#endif
                // fileManager.set_file_state(file_temp.filename, FileState::WAIT);
                file_temp.state = WAIT;
                pthread_mutex_unlock(&mut_arr[_index]);
                sem_post(&up_sem);
                usleep(500);
                continue;
            }
        }
        else
        {
#ifdef DE_BUG
            // printf("collet over\n");

            printf("search file\n");
#endif

            fileInfo &file_temp = fileManager.get_file_w();
            if (file_temp.state == OUT)
            {
                break;
            }
            curr_file = &file_temp;

#ifdef DE_BUG
            printf("filename:%s\n", file_temp.filename.c_str());
#endif
            switch (file_temp.filename[7])
            {
            case '0':
                _index = 0;
                break;
            case '1':
                _index = 1;
                break;
            }
            pthread_mutex_lock(&mut_arr[_index]);

            fp = fopen(file_temp.filename.c_str(), "w");

            if (fp != NULL)
            {

                for (size_t i = 0; i < PowerSpec_dBm.size(); i++)
                {
                    fprintf(fp, "%.6f\n", PowerSpec_dBm[i]);
                }
                fclose(fp);
                clock_gettime(CLOCK_MONOTONIC, &end);
                w_time = (end.tv_sec - start.tv_sec) * 1000.0;
                curr_file->state = READY;
                // fileManager.set_file_state(file_temp.filename, FileState::WAIT);
#ifdef DE_BUG
                printf("***************w_unlock****************\n");
#endif
                pthread_mutex_unlock(&mut_arr[_index]);
                sem_post(&up_sem);
            }
            else
            {
                printf("File is NULL\n");
                break;
            }
            usleep(500);
        }

        // 短暂休眠以避免忙等待
        if (!data_ready && !g_freq_changed)
        {
            usleep(100000); // 100ms
        }
    }

    printf("\nWriter: Shutting down...\n");

    // 清理资源
    sem_destroy(&up_sem);
    Device_Close(&Device);
    cleanup_shared_memory();

    printf("Writer: Thread terminated\n");
    return NULL;
}
