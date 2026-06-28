# Spectrum Analyzer Producer-Consumer System

基于共享内存 + 信号量的频谱仪数据采集与上传系统。

## 架构

```
 GUI (Python/PyQt5)          Controller (外部控制)
       │                           │
       │ 写入 freq/mode            │ SIGUSR1
       ▼                           ▼
 ┌──────────┐  sem_post  ┌──────────┐  SSH  ┌────────┐
 │ Producer │ ─────────▶ │ Uploader │ ────▶ │ Server │
 │ (C线程)  │            │ (C线程)  │       └────────┘
 └────┬─────┘            └────┬─────┘
      │                      │
      ▼                      ▼
 data0.txt / data1.txt  (旋转双缓冲)
      ▲
      │
 ┌──────────┐
 │ ipc_mgr  │  守护进程: 管理共享内存 + System V 信号量
 └──────────┘
```

## 组件

| 组件 | 文件 | 说明 |
|------|------|------|
| IPC 守护进程 | `src/ipc/ipc_mgr.c` | 管理共享内存/信号量生命周期 |
| IPC 客户端 | `src/ipc/ipc_client.c/h` | Unix socket 连接守护进程 |
| 共享内存 | `src/ipc/shm_ipc.c/h` | `SpectrumData` 结构体 + 操作函数 |
| **生产者** | `src/spectrum/producer.cpp/h` | 频谱仪数据采集线程 |
| **设备封装** | `src/spectrum/producer_device.cpp/h` | 封装 htra_api (SWP / IQS) |
| 上传者 | `src/spectrum/uploader.cpp/h` | SSH 上传文件到服务器 |
| 文件管理 | `src/spectrum/idefine_fun.cpp/h` | 旋转双缓冲 (data0↔data1) |
| 数据格式 | `src/spectrum/data_format.h` | 文件头格式定义 |
| GUI | `gui.py` | PyQt5 控制面板 |
| 主程序 | `src/spectrum/main.cpp` | 启动线程、初始化 IPC |

## 数据文件格式

每个文件 = **文本头** + 空行 + **二进制数据**:

```
#MAGIC:SPEC
#VERSION:1
#MODE:0                    (0=功率谱, 1=IQ)
#START_FREQ:13000000.000000
#STOP_FREQ:18000000.000000
#RBW:300000.000000
#POINTS:2048
#DATA_TYPE:float32          (float32 或 int16_interleaved)
#TIMESTAMP:1719500000

<binary data...>
```

- **功率谱**: `DATA_TYPE=float32`, `POINTS` 个 float (dBm)
- **IQ**: `DATA_TYPE=int16_interleaved`, `POINTS` 对 int16 (I0,Q0,I1,Q1,...)

## 模式切换流程

```
Controller → SHM: data_ready=1, mode=N, freq=f → SIGUSR1

Producer:
  1. 完成当前采集
  2. 写文件 + 头 (含当前 mode/freq) → READY → sem_post
  3. 读 SHM, 检测 mode 变化
  4. 关闭设备 → 以新模式打开 → 继续采集

Uploader: 无感知。每文件自描述 mode/freq
```

## 目录结构

```
/home/scd/ok/
├── src/
│   ├── ipc/               ← IPC 通信 (守护进程+客户端)
│   │   ├── ipc_mgr.c/h
│   │   ├── ipc_client.c/h
│   │   └── shm_ipc.c/h
│   └── spectrum/           ← 频谱仪采集程序
│       ├── main.cpp
│       ├── producer.cpp/h
│       ├── producer_device.cpp/h
│       ├── uploader.cpp/h
│       ├── idefine_fun.cpp/h
│       ├── data_format.h
│       └── Error_handling.cpp
├── ref/                    ← 参考代码 (不参与编译)
│   ├── IQS_GetIQToTxt.cpp
│   └── previous_porducer.cpp
├── gui.py
├── start.sh
├── build/                  ← 编译产物
├── logs/                   ← 运行时日志
└── data0.txt / data1.txt   ← 运行时数据文件
```

## 编译

`start.sh` 一键编译启动:

```bash
./start.sh start     # 编译 + 启动
./start.sh stop      # 停止 + 清理
./start.sh status    # 查看状态
```

手动编译:

```bash
# IPC 守护进程
gcc -o build/ipc_mgr src/ipc/ipc_mgr.c src/ipc/shm_ipc.c \
    -Isrc -lpthread

# 频谱仪采集程序
g++ -std=c++11 -pthread -o build/spectrum \
    src/spectrum/main.cpp \
    src/spectrum/producer.cpp \
    src/spectrum/producer_device.cpp \
    src/spectrum/uploader.cpp \
    src/spectrum/idefine_fun.cpp \
    src/spectrum/Error_handling.cpp \
    src/ipc/ipc_client.c src/ipc/shm_ipc.c \
    -Isrc -Isrc/ipc -Isrc/spectrum -I/opt/htraapi/inc \
    -L/opt/htraapi/lib/x86_64 -Wl,-rpath='/opt/htraapi/lib/x86_64' \
    -lhtraapi -lssh2 -lrt
```

## 运行

```bash
./start.sh start
```

或手动:

```bash
./build/ipc_mgr &
./build/spectrum &
python3 gui.py
```

## IQ 模式频率约定

`SpectrumData` 只有 `start_freq` / `stop_freq`。IQ 模式下将 `start_freq` 视作中心频率，`stop_freq` 忽略。文件头中 `STOP_FREQ=0`, `RBW=0`。
