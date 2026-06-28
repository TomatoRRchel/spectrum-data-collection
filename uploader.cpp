#include "uploader.h"
#include "data_format.h"
#include "idefine_fun.h"

void *uploader_thread(void *arg)
{
    UploaderArgs *args = (UploaderArgs *)arg;
    signal(SIGINT, SIG_IGN); // 忽略 SIGINT，交给主线程处理

    file_Manager *file_mgr = args->file_mgr;

    printf("[UPLOADER] Thread started\n");

    while (!atomic_load(args->shutdown))
    {
        // 等待生产者线程的信号
        sem_wait(args->upload_sem);

        // 再次检查退出标志（可能被 sem_post 唤醒用于退出）
        if (atomic_load(args->shutdown))
            break;

        fileInfo *f = file_mgr->get_file_r();
        if (f->state == OUT)
        {
            printf("[UPLOADER] No more files, exiting.\n");
            break;
        }

        if (f->state == READY)
        {
            // ----- 读取并打印文件头部信息 (调试/日志用) -----
            FILE *fp = fopen(f->filename.c_str(), "rb");
            if (fp)
            {
                ParsedHeader hdr;
                if (parse_data_header(fp, &hdr) == 0)
                {
                    printf("[UPLOADER] Uploading %s: mode=%d, "
                           "freq=%.3f-%.3f Hz, points=%u, type=%s\n",
                           f->filename.c_str(),
                           hdr.mode,
                           hdr.start_freq,
                           hdr.stop_freq,
                           hdr.points,
                           hdr.data_type == DATA_TYPE_FLOAT32 ? "float32" : "int16_interleaved");
                }
                fclose(fp);
            }

            // ----- 上传文件到服务器 -----
            // 使用 Up_Class SSH 上传; 如需接入真实的 Up_Class,
            // 在此处创建 Up_Class 实例并调用 ssh_upload()。
            // 示例:
            //   Up_Class uploader("/remote/path");
            //   string local = f->filename;
            //   uploader.ssh_upload(local, false);

            printf("[UPLOADER] Uploading: %s\n", f->filename.c_str());
            usleep(100000); // 模拟上传时间 (实际使用时替换为真实 SSH 上传)

            // 上传完成后，标记文件为可重用
            f->state = WAIT;
            printf("[UPLOADER] Done: %s\n", f->filename.c_str());
        }
    }

    printf("[UPLOADER] Thread exiting.\n");
    return NULL;
}