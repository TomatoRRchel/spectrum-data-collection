#!/bin/bash

# ============================================================
# 频谱仪系统启动脚本（修正版 - 启动 GUI 前端）
# ============================================================

set -e

# ==================== 配置 ====================
BUILD_DIR="./build"
LOG_DIR="./logs"
IPC_DAEMON="ipc_mgr"
SPECTRUM_APP="spectrum"                # 频谱仪采集程序
GUI_SCRIPT="gui.py"                    # 前端脚本
IPC_SOCKET_PATH="/tmp/ipc_mgr_socket"

SHM_KEY_HEX="1234"
SEM_KEY_HEX="5678"
WAIT_TIMEOUT=30

# ==================== 颜色输出 ====================
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m'

log_info() { echo -e "${BLUE}[INFO]${NC} $1"; }
log_success() { echo -e "${GREEN}[SUCCESS]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

command_exists() { command -v "$1" >/dev/null 2>&1; }

S_DIR="src"
IPC_DIR="${S_DIR}/ipc"
SPECTRUM_DIR="${S_DIR}/spectrum"
HTRA_SDK="/opt/htraapi"
TARG="x86_64"

# ==================== 编译 ====================
compile_ipc_daemon() {
    log_info "编译 IPC 守护进程..."
    mkdir -p "$BUILD_DIR"
    gcc -o "$BUILD_DIR/$IPC_DAEMON" \
        ${IPC_DIR}/ipc_mgr.c ${IPC_DIR}/shm_ipc.c \
        -I${S_DIR} -lpthread || return 1
    log_success "编译完成: $BUILD_DIR/$IPC_DAEMON"
}

compile_spectrum_app() {
    log_info "编译频谱仪采集程序..."
    local HTRA_INC="-I${S_DIR} -I${IPC_DIR} -I${SPECTRUM_DIR} -I${HTRA_SDK}/inc"
    local HTRA_LIB="-L${HTRA_SDK}/lib/${TARG} -Wl,-rpath='${HTRA_SDK}/lib/${TARG}'"
    g++ -std=c++11 -pthread -o "$BUILD_DIR/$SPECTRUM_APP" \
        ${SPECTRUM_DIR}/main.cpp \
        ${SPECTRUM_DIR}/producer.cpp \
        ${SPECTRUM_DIR}/producer_device.cpp \
        ${SPECTRUM_DIR}/uploader.cpp \
        ${SPECTRUM_DIR}/idefine_fun.cpp \
        ${SPECTRUM_DIR}/Error_handling.cpp \
        ${IPC_DIR}/ipc_client.c \
        ${IPC_DIR}/shm_ipc.c \
        $HTRA_INC $HTRA_LIB -lhtraapi -lssh2 -lrt || return 1
    log_success "编译完成: $BUILD_DIR/$SPECTRUM_APP"
}

# ==================== IPC 资源清理 ====================
cleanup_ipc_resources() {
    log_info "清理旧的 IPC 资源..."
    ipcs -m 2>/dev/null | awk -v key="$SHM_KEY_HEX" '$2 ~ /0x/ && $2 ~ key {print $2}' | while read id; do
        ipcrm -m "$id" 2>/dev/null && log_info "删除共享内存 $id"
    done
    ipcs -s 2>/dev/null | awk -v key="$SEM_KEY_HEX" '$2 ~ /0x/ && $2 ~ key {print $2}' | while read id; do
        ipcrm -s "$id" 2>/dev/null && log_info "删除信号量 $id"
    done
    if [ -S "$IPC_SOCKET_PATH" ]; then
        rm -f "$IPC_SOCKET_PATH"
        log_info "删除 socket: $IPC_SOCKET_PATH"
    fi
}

# ==================== 资源就绪检查 ====================
check_shm_ready() {
    ipcs -m 2>/dev/null | grep -q "0x[0]*${SHM_KEY_HEX}"
}

check_sem_ready() {
    ipcs -s 2>/dev/null | grep -q "0x[0]*${SEM_KEY_HEX}"
}

wait_for_ipc_resources() {
    local attempt=0
    log_info "等待 IPC 资源创建（最多 ${WAIT_TIMEOUT} 秒）..."
    while [ $attempt -lt "$WAIT_TIMEOUT" ]; do
        if check_shm_ready && check_sem_ready && [ -S "$IPC_SOCKET_PATH" ]; then
            echo
            log_success "IPC 资源全部就绪"
            return 0
        fi
        sleep 1
        ((attempt++))
        echo -n "."
    done
    echo
    log_error "等待超时 (${WAIT_TIMEOUT} 秒)"
    return 1
}

# ==================== 启动守护进程（新终端） ====================
start_ipc_daemon_terminal() {
    log_info "启动 IPC 守护进程..."
    cleanup_ipc_resources

    if command_exists gnome-terminal; then
        gnome-terminal --title="IPC 守护进程" -- bash -c "
            echo '守护进程启动...'
            echo '共享内存键: 0x${SHM_KEY_HEX}  信号量键: 0x${SEM_KEY_HEX}'
            echo 'Socket: ${IPC_SOCKET_PATH}'
            echo '----------------------------------------'
            $BUILD_DIR/$IPC_DAEMON
            echo '守护进程已退出，按回车关闭'
            read
        " &
    elif command_exists xterm; then
        xterm -title "IPC 守护进程" -e bash -c "
            $BUILD_DIR/$IPC_DAEMON
            echo '按回车关闭'
            read
        " &
    else
        mkdir -p "$LOG_DIR"
        nohup "$BUILD_DIR/$IPC_DAEMON" > "$LOG_DIR/ipc_daemon.log" 2>&1 &
        echo $! > "$LOG_DIR/ipc_daemon.pid"
        log_info "后台运行，PID: $(cat $LOG_DIR/ipc_daemon.pid)"
    fi

    wait_for_ipc_resources || exit 1
    log_success "守护进程就绪"
}

# ==================== 启动 GUI 前端（新终端） ====================
# 修改点：新增函数，用于启动 gui.py
start_gui_terminal() {
    log_info "启动 GUI 前端界面..."
    if command_exists gnome-terminal; then
        gnome-terminal --title="频谱仪前端 GUI" -- bash -c "
            echo '前端界面启动...'
            echo 'Socket: ${IPC_SOCKET_PATH}'
            echo '----------------------------------------'
            python3 $GUI_SCRIPT
            echo '前端已退出，按回车关闭'
            read
        " &
    elif command_exists xterm; then
        xterm -title "频谱仪前端 GUI" -e bash -c "
            python3 $GUI_SCRIPT
            echo '按回车关闭'
            read
        " &
    else
        mkdir -p "$LOG_DIR"
        nohup python3 "$GUI_SCRIPT" > "$LOG_DIR/gui.log" 2>&1 &
        echo $! > "$LOG_DIR/gui.pid"
        log_info "后台运行，PID: $(cat $LOG_DIR/gui.pid)"
    fi
    log_success "GUI 前端已启动"
}

# ==================== 停止与状态 ====================
stop_process_by_pattern() {
    local pattern=$1
    local name=$2
    local pids=$(pgrep -f "$pattern" 2>/dev/null)
    if [ -z "$pids" ]; then
        log_warn "$name 未运行"
        return 0
    fi
    log_info "停止 $name (PID: $pids)..."
    echo "$pids" | xargs kill -TERM 2>/dev/null
    local waited=0
    while [ $waited -lt 3 ]; do
        if ! pgrep -f "$pattern" >/dev/null; then
            log_success "$name 已正常退出"
            return 0
        fi
        sleep 1
        ((waited++))
    done
    # 超时则强制杀死
    log_warn "$name 未响应 SIGTERM，强制终止"
    echo "$pids" | xargs kill -KILL 2>/dev/null
    sleep 1
}

# 修改 stop_all 函数
stop_all() {
    log_info "停止所有进程..."
    # 先停止 GUI 前端（它会自动停止其启动的采集程序）
    stop_process_by_pattern "$GUI_SCRIPT" "GUI 前端"
    # 再停止守护进程（它会自行清理 IPC 资源）
    stop_process_by_pattern "$BUILD_DIR/$IPC_DAEMON" "IPC 守护进程"
    # 最后清理可能残留的 IPC 资源（通常守护进程已清理，此步为保险）
    cleanup_ipc_resources
    log_success "已停止并清理"
}

show_status() {
    echo
    log_info "========== 系统状态 =========="
    pgrep -f "$BUILD_DIR/$IPC_DAEMON" >/dev/null && log_success "✓ 守护进程运行中" || log_warn "✗ 守护进程未运行"
    pgrep -f "$BUILD_DIR/$SPECTRUM_APP" >/dev/null && log_success "✓ 采集程序运行中" || log_warn "✗ 采集程序未运行"
    pgrep -f "$GUI_SCRIPT" >/dev/null && log_success "✓ GUI 前端运行中" || log_warn "✗ GUI 前端未运行"
    echo
    log_info "共享内存 (键 0x$SHM_KEY_HEX):"
    ipcs -m | grep "0x[0]*${SHM_KEY_HEX}" || echo "  无"
    echo
    log_info "信号量 (键 0x$SEM_KEY_HEX):"
    ipcs -s | grep "0x[0]*${SEM_KEY_HEX}" || echo "  无"
    echo
    [ -S "$IPC_SOCKET_PATH" ] && log_success "Socket: $IPC_SOCKET_PATH 存在" || log_warn "Socket: $IPC_SOCKET_PATH 不存在"
}

# ==================== 主流程 ====================
main() {
    case "$1" in
        start)
            mkdir -p "$BUILD_DIR" "$LOG_DIR"
            compile_ipc_daemon || exit 1
            compile_spectrum_app || exit 1      # 保留编译频谱仪采集程序
            start_ipc_daemon_terminal
            # 修改点：启动 GUI 终端，替代原来的 start_spectrum_terminal
            start_gui_terminal
            show_status
            ;;
        stop)
            stop_all
            ;;
        restart)
            stop_all
            sleep 2
            "$0" start
            ;;
        status)
            show_status
            ;;
        clean)
            stop_all
            rm -rf "$BUILD_DIR"/* "$LOG_DIR"/* 2>/dev/null || true
            log_success "清理完成"
            ;;
        *)
            echo "用法: $0 {start|stop|restart|status|clean}"
            exit 1
            ;;
    esac
}

main "$@"