#include "uploader.h"
#include "data_format.h"
#include "idefine_fun.h"

void *uploader_thread(void *arg)
{
    UploaderArgs *a = (UploaderArgs *)arg;
    signal(SIGINT, SIG_IGN);
    file_Manager *fm = a->file_mgr;
    printf("[UP] Started\n");

    while (!atomic_load(a->shutdown))
    {
        sem_wait(a->upload_sem);
        if (atomic_load(a->shutdown))
            break;

        fileInfo *f = fm->get_file_r();
        if (f->state == OUT)
        {
            printf("[UP] No files, exit\n");
            break;
        }
        if (f->state != READY)
            continue;

        // 读头部日志
        FILE *fp = fopen(f->filename.c_str(), "rb");
        if (fp)
        {
            ParsedHeader h;
            if (!parse_data_header(fp, &h))
                printf("[UP] %s mode=%d freq=%.3f-%.3f pts=%u type=%s\n",
                       f->filename.c_str(), h.mode, h.start_freq, h.stop_freq,
                       h.points,
                       h.data_type == DATA_TYPE_FLOAT32 ? "float32" : "int16");
            fclose(fp);
        }

        // 上传 (接入 Up_Class::ssh_upload 替换 usleep)
        usleep(100000);
        f->state = WAIT;
        printf("[UP] Done: %s\n", f->filename.c_str());
    }
    printf("[UP] Exit\n");
    return NULL;
}