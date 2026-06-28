#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#include "shm_ipc.h"
#include "producer.h" // 生产者线程函数声明
#include "uploader.h" // 上传线程函数声明
#include "ipc_client.h"
// #include "consumer.h" // 消费者线程函数声明
atomic_bool g_shutdown = ATOMIC_VAR_INIT(0);
// 全局变量
// static volatile sig_atomic_t g_shutdown = 0;
static SpectrumData *g_shm_data = NULL;
static int g_sem_id = -1;
static int g_shm_id = -1;
static file_Manager g_file_mgr; // 文件管理器实例
ipc_client_t g_ipc;				// IPC 客户端结构体实例
// 线程同步信号量 (用于触发消费者上传)
static sem_t g_upload_sem;

// 信号处理函数
static void signal_handler(int sig)
{
	if (sig == SIGINT || sig == SIGTERM)
	{
		atomic_store(&g_shutdown, 1);
		// 如果消费者阻塞在 sem_wait 上，post 一次使其唤醒检查退出标志
		sem_post(&g_upload_sem);
	}
}

void *heartbeat_thread(void *arg)
{
	while (!atomic_load(&g_shutdown))
	{
		ipc_client_heartbeat(&g_ipc);
		sleep(2);
	}
	return NULL;
}

// 生产者线程参数结构
// typedef struct
// {
// 	SpectrumData *shm;
// 	int sem_id;
// 	file_Manager *file_mgr;
// 	sem_t *upload_sem;
// } ProducerArgs;

// 消费者线程参数结构
// typedef struct
// {
// 	SpectrumData *shm;
// 	file_Manager *file_mgr;
// 	sem_t *upload_sem;
// } ConsumerArgs;
void clean()
{

	if (sem_destroy(&g_upload_sem) != 0)
	{
		perror("sem_destroy upload_sem");
	}
}
int main(void)
{

	ProducerArgs prod_args;
	UploaderArgs cons_args;
	pthread_t uploader_tid, spectrum_tid;
	// 设置信号处理
	struct sigaction sa;
	sa.sa_handler = signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);
	// 忽略 SIGUSR1/SIGUSR2，由生产者线程内部处理
	signal(SIGUSR1, SIG_IGN);
	signal(SIGUSR2, SIG_IGN);

	if (ipc_client_init(&g_ipc, CLIENT_LONG_TERM) != 0)
	{
		return 1;
	}
	printf("register ok\n");
	g_shm_data = (SpectrumData *)g_ipc.shm_ptr;

	if (sem_init(&g_upload_sem, 0, 0) != 0)
	{
		perror("sem_init upload_sem");
		destroy_semaphore(g_sem_id);
		detach_shared_memory(g_shm_data);
		return -6;
	}
	prod_args.shutdown = &g_shutdown;
	prod_args.shm = g_shm_data;
	prod_args.sem_id = g_ipc.sem_id;
	prod_args.file_mgr = &g_file_mgr;
	prod_args.upload_sem = &g_upload_sem;

	// cons_args.shm = g_shm_data;
	cons_args.shutdown = &g_shutdown;
	cons_args.file_mgr = &g_file_mgr;
	cons_args.upload_sem = &g_upload_sem;

	// if (pthread_create(&uploader_tid, NULL, uploader_thread, &cons_args) != 0)
	// {
	// 	perror("pthread_create uploader");
	// 	clean();
	// 	return EXIT_FAILURE;
	// }

	if (pthread_create(&spectrum_tid, NULL, spectrum_thread, &prod_args) != 0)
	{
		perror("pthread_create producer");
		clean();
		return EXIT_FAILURE;
	}
	printf("Producer thread started.\n");
	pthread_t heartbeat_tid;
	if (pthread_create(&heartbeat_tid, NULL, heartbeat_thread, NULL) != 0)
	{
		perror("pthread_create heartbeat");
		g_shutdown = 1;
	}
	printf("System started. Press Ctrl+C to stop.\n");
	pthread_join(spectrum_tid, NULL);
	ipc_client_close(&g_ipc); // 生产者线程已获取共享内存和信号量信息，主线程可以关闭 IPC 客户端连接
	// 创建消费者线程
	// if (pthread_create(&consumer_tid, NULL, consumer_thread, &cons_args) != 0)
	// {
	// 	perror("pthread_create consumer");
	// 	g_shutdown = 1;
	// 	sem_post(&g_upload_sem); // 唤醒可能阻塞的生产者
	// 	pthread_join(producer_tid, NULL);
	// 	cleanup_ipc_resources();
	// 	return EXIT_FAILURE;
	// }

	// 等待线程结束

	// pthread_join(spectrum_tid, NULL);
	pthread_join(uploader_tid, NULL);
	clean();
	printf("System terminated gracefully.\n");
	return EXIT_SUCCESS;
}