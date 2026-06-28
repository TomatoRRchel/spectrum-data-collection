#ifndef DATA_FORMAT_H
#define DATA_FORMAT_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

// ====================== 数据文件格式定义 ======================
// 每个数据文件 = 文本头(ASCII) + 空行 + 二进制数据
//
// 文本头格式:
//   #MAGIC:SPEC
//   #VERSION:1
//   #MODE:0|1
//   #START_FREQ:Hz
//   #STOP_FREQ:Hz
//   #RBW:Hz
//   #POINTS:N
//   #DATA_TYPE:float32|int16_interleaved
//   #TIMESTAMP:unix_time
//   #CHECKSUM:crc32_hex
//   (空行)
//   <binary data>
//
// MODE=0: 功率谱, DATA_TYPE=float32, POINTS个float值
// MODE=1: IQ, DATA_TYPE=int16_interleaved, POINTS对 int16_t (I0,Q0,I1,Q1,...)
// =============================================================

#ifdef __cplusplus
extern "C"
{
#endif

    // 最大头部大小（足够容纳所有元数据行）
#define MAX_HEADER_SIZE 512

    // 魔数标识
#define DATA_MAGIC "SPEC"
#define DATA_VERSION 1

    // 数据类型标识
#define DATA_TYPE_FLOAT32 0
#define DATA_TYPE_INT16_INTERLEAVED 1

    /**
     * 将数据文件的文本头写入 fp
     * @param fp         已打开的文件指针
     * @param mode       模式: 0=功率谱, 1=IQ
     * @param start_freq 起始频率 (Hz)
     * @param stop_freq  终止频率 (Hz)
     * @param rbw_hz     分辨率带宽 (Hz), IQ模式传0
     * @param data_type  DATA_TYPE_FLOAT32 或 DATA_TYPE_INT16_INTERLEAVED
     * @param points     数据点数
     * @return 写入的头部字节数, 失败返回-1
     */
    static inline int write_data_header(FILE *fp,
                                        int mode,
                                        double start_freq,
                                        double stop_freq,
                                        double rbw_hz,
                                        int data_type,
                                        uint32_t points)
    {
        time_t now = time(NULL);
        int written = fprintf(fp,
                              "#MAGIC:" DATA_MAGIC "\n"
                              "#VERSION:%d\n"
                              "#MODE:%d\n"
                              "#START_FREQ:%.6f\n"
                              "#STOP_FREQ:%.6f\n"
                              "#RBW:%.6f\n"
                              "#POINTS:%u\n"
                              "#DATA_TYPE:%s\n"
                              "#TIMESTAMP:%ld\n"
                              "\n",
                              DATA_VERSION,
                              mode,
                              start_freq,
                              stop_freq,
                              rbw_hz,
                              points,
                              data_type == DATA_TYPE_FLOAT32 ? "float32" : "int16_interleaved",
                              (long)now);
        if (written < 0)
            return -1;
        fflush(fp);
        return written;
    }

    /**
     * 从文件中读取头部信息（服务器端用）
     * 解析后的结构体
     */
    typedef struct
    {
        int mode;            // 0=功率谱, 1=IQ
        double start_freq;   // Hz
        double stop_freq;    // Hz
        double rbw_hz;       // Hz
        int data_type;       // DATA_TYPE_FLOAT32 / DATA_TYPE_INT16_INTERLEAVED
        uint32_t points;     // 数据点数
        time_t timestamp;    // Unix时间戳
        long data_offset;    // 二进制数据在文件中的偏移量
    } ParsedHeader;

    /**
     * 从已打开的文件中解析头部
     * @param fp  已打开的文件指针（读取位置应为文件开头）
     * @param out 输出解析结果
     * @return 0=成功, -1=格式错误
     */
    static inline int parse_data_header(FILE *fp, ParsedHeader *out)
    {
        char line[256];
        int fields = 0;
        memset(out, 0, sizeof(*out));

        rewind(fp);
        while (fgets(line, sizeof(line), fp))
        {
            // 遇到空行，头部结束
            if (line[0] == '\n' || line[0] == '\r' || line[0] == '\0')
            {
                out->data_offset = ftell(fp);
                return (fields >= 7) ? 0 : -1;
            }

            char value[128];
            if (sscanf(line, "#MODE:%d", &out->mode) == 1)
                fields++;
            else if (sscanf(line, "#START_FREQ:%lf", &out->start_freq) == 1)
                fields++;
            else if (sscanf(line, "#STOP_FREQ:%lf", &out->stop_freq) == 1)
                fields++;
            else if (sscanf(line, "#RBW:%lf", &out->rbw_hz) == 1)
                fields++;
            else if (sscanf(line, "#POINTS:%u", &out->points) == 1)
                fields++;
            else if (sscanf(line, "#DATA_TYPE:%127s", value) == 1)
            {
                if (strcmp(value, "float32") == 0)
                    out->data_type = DATA_TYPE_FLOAT32;
                else if (strcmp(value, "int16_interleaved") == 0)
                    out->data_type = DATA_TYPE_INT16_INTERLEAVED;
                fields++;
            }
            else if (sscanf(line, "#TIMESTAMP:%ld", &out->timestamp) == 1)
                fields++;
        }

        return -1; // 没有找到空行分隔符
    }

#ifdef __cplusplus
}
#endif

#endif // DATA_FORMAT_H
