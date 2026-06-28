// struct_info.c
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include <sys/ipc.h>

// 从你的 ipc_mgr.h 复制结构定义
typedef enum
{
    CLIENT_LONG_TERM = 1, // 长期进程（USER1, USER2）
    CLIENT_TRANSIENT = 2  // 临时进程（USER3）
} client_type_t;

// 请求类型
typedef enum
{
    REQ_REGISTER = 1, // 注册客户端
    REQ_GET_SHM_INFO, // 获取共享内存信息（key, id, size, 信号量ID）
    REQ_HEARTBEAT,    // 心跳（长期进程）
    REQ_UNREGISTER,   // 注销客户端
    REQ_SHUTDOWN      // 通知守护进程退出（可选）
} request_type_t;

// 响应状态
typedef enum
{
    RESP_OK = 0,
    RESP_ERR_NOT_REGISTERED,
    RESP_ERR_IPC_NOT_READY,
    RESP_ERR_UNKNOWN
} response_status_t;

// 请求结构
typedef struct
{
    request_type_t type;
    client_type_t client_type; // 仅 REGISTER 时有效
    pid_t pid;                 // 客户端 PID
    uint32_t seq;              // 序列号，用于匹配响应
} ipc_request_t;

// 响应结构
typedef struct
{
    uint32_t seq; // 对应请求的序列号
    response_status_t status;
    // 当 type = REQ_GET_SHM_INFO 时有效
    key_t shm_key;
    int shm_id;
    size_t shm_size;
    key_t sem_key;
    int sem_id;
} ipc_response_t;

// 打印单个成员的信息
void print_member_info(const char *struct_name, const char *member_name,
                       size_t offset, size_t size, size_t align)
{
    printf("  %-20s offset: %3zu, size: %2zu, align: %zu",
           member_name, offset, size, align);

    // 显示类型提示
    if (size == 1)
        printf(" (uint8_t/char)");
    else if (size == 2)
        printf(" (uint16_t)");
    else if (size == 4)
        printf(" (uint32_t/int)");
    else if (size == 8)
        printf(" (uint64_t/long/pointer)");

    printf("\n");
}

// 打印结构体信息
void print_struct_info(const char *name, size_t total_size, size_t align)
{
    printf("\n================== %s ==================\n", name);
    printf("总大小: %zu 字节\n", total_size);
    printf("对齐要求: %zu 字节\n", align);
    printf("内存布局:\n");
}

int main()
{
    // 获取系统信息
    printf("系统信息:\n");
    printf("  sizeof(void*) = %zu (指针大小)\n", sizeof(void *));
    printf("  sizeof(size_t) = %zu\n", sizeof(size_t));
    printf("  sizeof(pid_t) = %zu\n", sizeof(pid_t));
    printf("  sizeof(key_t) = %zu\n", sizeof(key_t));
    printf("  sizeof(int) = %zu\n", sizeof(int));
    printf("\n");

    // 获取 ipc_request_t 的信息
    print_struct_info("ipc_request_t", sizeof(ipc_request_t), _Alignof(ipc_request_t));

    print_member_info("ipc_request_t", "type",
                      offsetof(ipc_request_t, type),
                      sizeof(request_type_t),
                      _Alignof(request_type_t));

    print_member_info("ipc_request_t", "client_type",
                      offsetof(ipc_request_t, client_type),
                      sizeof(client_type_t),
                      _Alignof(client_type_t));

    print_member_info("ipc_request_t", "pid",
                      offsetof(ipc_request_t, pid),
                      sizeof(pid_t),
                      _Alignof(pid_t));

    print_member_info("ipc_request_t", "seq",
                      offsetof(ipc_request_t, seq),
                      sizeof(uint32_t),
                      _Alignof(uint32_t));

    // 获取 ipc_response_t 的信息
    print_struct_info("ipc_response_t", sizeof(ipc_response_t), _Alignof(ipc_response_t));

    print_member_info("ipc_response_t", "seq",
                      offsetof(ipc_response_t, seq),
                      sizeof(uint32_t),
                      _Alignof(uint32_t));

    print_member_info("ipc_response_t", "status",
                      offsetof(ipc_response_t, status),
                      sizeof(response_status_t),
                      _Alignof(response_status_t));

    print_member_info("ipc_response_t", "shm_key",
                      offsetof(ipc_response_t, shm_key),
                      sizeof(key_t),
                      _Alignof(key_t));

    print_member_info("ipc_response_t", "shm_id",
                      offsetof(ipc_response_t, shm_id),
                      sizeof(int),
                      _Alignof(int));

    print_member_info("ipc_response_t", "shm_size",
                      offsetof(ipc_response_t, shm_size),
                      sizeof(size_t),
                      _Alignof(size_t));

    print_member_info("ipc_response_t", "sem_key",
                      offsetof(ipc_response_t, sem_key),
                      sizeof(key_t),
                      _Alignof(key_t));

    print_member_info("ipc_response_t", "sem_id",
                      offsetof(ipc_response_t, sem_id),
                      sizeof(int),
                      _Alignof(int));

    // 输出用于 Python 的 struct 格式字符串
    printf("\n================== Python struct 格式 ==================\n");

    // ipc_request_t
    printf("ipc_request_t format string: ");
    printf("'");

    // 根据偏移量自动生成格式字符串
    size_t request_offsets[] = {
        offsetof(ipc_request_t, type),
        offsetof(ipc_request_t, client_type),
        offsetof(ipc_request_t, pid),
        offsetof(ipc_request_t, seq)};

    // 计算填充
    size_t current_offset = 0;
    for (int i = 0; i < 4; i++)
    {
        size_t offset = request_offsets[i];
        if (current_offset < offset)
        {
            printf("x%zu", offset - current_offset); // 填充字节
        }
        current_offset = offset;
    }

    // 添加成员格式
    printf("I"   // type (假设是4字节int)
           "I"   // client_type
           "I"   // pid (假设是4字节int)
           "I"); // seq

    // 处理尾部填充
    if (current_offset + 4 * sizeof(int) < sizeof(ipc_request_t))
    {
        printf("x%zu", sizeof(ipc_request_t) - (current_offset + 4 * sizeof(int)));
    }

    printf("'\n");

    // 简单版本：假设没有填充
    printf("简化版（假设无填充）: 'IIII'  # type, client_type, pid, seq\n");
    printf("实际大小: %zu 字节\n", sizeof(ipc_request_t));

    // ipc_response_t
    printf("\nipc_response_t format string: ");
    printf("'I"  // seq
           "I"   // status
           "i"   // shm_key (有符号int)
           "i"   // shm_id
           "N"   // shm_size (平台相关的size_t，不推荐用struct)
           "i"   // sem_key
           "i"); // sem_id
    printf("'\n");

    printf("注意: size_t 的大小和符号取决于平台，建议单独处理\n");

    // 输出十六进制内存布局
    printf("\n================== 十六进制内存布局 ==================\n");

    ipc_request_t req = {0};
    printf("ipc_request_t 内存示例（全0）:\n");
    unsigned char *p = (unsigned char *)&req;
    for (size_t i = 0; i < sizeof(ipc_request_t); i++)
    {
        if (i % 4 == 0)
            printf("\n  [%03zu]: ", i);
        printf("%02x ", p[i]);
    }
    printf("\n");

    printf("\n========== 编译选项建议 ==========\n");
    printf("使用 #pragma pack 确保跨平台一致性:\n");
    printf("#pragma pack(push, 1)\n");
    printf("typedef struct {\n");
    printf("    request_type_t type;\n");
    printf("    client_type_t client_type;\n");
    printf("    pid_t pid;\n");
    printf("    uint32_t seq;\n");
    printf("} __attribute__((packed)) ipc_request_t;\n");
    printf("#pragma pack(pop)\n");

    return 0;
}