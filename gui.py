#!/usr/bin/env python3
"""
Spectrum Control GUI - PyQt5 Interface for Spectrum Analyzer Simulation
通过守护进程管理共享内存，与频谱仪进程通信
"""

import sys
import os
import signal
import struct
import time
import ctypes
import subprocess
import socket
import threading
import select
from PyQt5.QtWidgets import (
    QApplication, QMainWindow, QWidget, QVBoxLayout, QHBoxLayout,
    QLabel, QLineEdit, QPushButton, QMessageBox, QGroupBox,
    QTextEdit, QGridLayout, QComboBox, QStatusBar
)
from PyQt5.QtCore import Qt, QTimer, QThread, pyqtSignal, QObject
from PyQt5.QtGui import QFont, QDoubleValidator

# 尝试导入 sysv_ipc
try:
    import sysv_ipc
except ImportError:
    print("请安装 sysv_ipc 库: pip install sysv_ipc")
    sys.exit(1)

# ================== IPC 协议常量 ==================
IPC_SOCKET_PATH = "/tmp/ipc_mgr_socket"

# 请求类型
REQ_REGISTER = 1
REQ_GET_SHM_INFO = 2
REQ_HEARTBEAT = 3
REQ_UNREGISTER = 4

# 客户端类型
CLIENT_LONG_TERM = 1
CLIENT_TRANSIENT = 2

# 响应状态
RESP_OK = 0
RESP_ERR_NOT_REGISTERED = 1
RESP_ERR_IPC_NOT_READY = 2
RESP_ERR_UNKNOWN = 3

# 共享内存 Key (与 C 代码一致)
SHM_KEY = 0x1234
SEM_KEY = 0x5678

# 心跳间隔 (秒)
HEARTBEAT_INTERVAL = 3

# ================== 共享内存数据结构 ==================
class SpectrumData(ctypes.Structure):
    _fields_ = [
        ("start_freq", ctypes.c_double),      # 起始频率 (Hz)
        ("stop_freq", ctypes.c_double),       # 中止频率 (Hz)
        ("mode", ctypes.c_uint),              # 工作模式 (0=功率谱, 1=IQ)
        ("ref_count", ctypes.c_uint),         # 引用计数
        ("controller_pid", ctypes.c_int),     # 生产者进程ID
        ("spectrum_pid", ctypes.c_int),       # 消费者进程ID
        ("sequence", ctypes.c_uint32),        # 数据序列号
        ("timestamp", ctypes.c_int64),        # 更新时间戳
        ("heartbeat", ctypes.c_int64),        # 生产者心跳
        ("data_ready", ctypes.c_uint8),       # 数据就绪标志
        ("shutdown", ctypes.c_uint8),         # 关闭标志
    ]

# ================== 守护进程通信类 ==================
class DaemonClient:
    def __init__(self):
        self.sock = None
        self.seq = 0
        self.registered = False
        self.shm = None
        self.sem = None
        self.shm_id = None
        self.sem_id = None

    def connect(self):
        try:
            self.sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            self.sock.connect(IPC_SOCKET_PATH)
            self.sock.settimeout(2.0)   # 设置超时避免永久阻塞
            return True
        except Exception as e:
            print(f"连接守护进程失败: {e}")
            return False

    def _send_request(self, req_type, client_type=CLIENT_LONG_TERM, pid=None):
        if not self.sock:
            return None
        self.seq += 1
        pid_val = pid if pid is not None else os.getpid()
        # 打包 ipc_request_t: 4个 uint32_t (小端)
        req_packed = struct.pack('<IIII', req_type, client_type, pid_val, self.seq)
        self.sock.send(req_packed)

        # 接收 ipc_response_t: 共32字节，格式: seq(4), status(4), shm_key(4), shm_id(4), shm_size(8), sem_key(4), sem_id(4)
        try:
            resp_data = self.sock.recv(32)
            if len(resp_data) != 32:
                return None
            # 解包：4个I, 1个Q, 2个I
            seq, status, shm_key, shm_id, shm_size, sem_key, sem_id = struct.unpack('<III I Q I I', resp_data)
            # 注意：上面格式 '<III I Q I I' 等价于 '<IIIIQII'
            return {
                'seq': seq,
                'status': status,
                'shm_key': shm_key,
                'shm_id': shm_id,
                'shm_size': shm_size,
                'sem_key': sem_key,
                'sem_id': sem_id
            }
        except socket.timeout:
            print("接收响应超时")
            return None
        except Exception as e:
            print(f"接收响应错误: {e}")
            return None

    def register(self):
        resp = self._send_request(REQ_REGISTER, CLIENT_LONG_TERM)
        if resp and resp['status'] == RESP_OK:
            self.registered = True
            return True
        return False

    def get_shm_info(self):
        if not self.registered:
            return None
        resp = self._send_request(REQ_GET_SHM_INFO)
        if resp and resp['status'] == RESP_OK:
            return resp
        return None

    def send_heartbeat(self):
        if not self.registered:
            return False
        resp = self._send_request(REQ_HEARTBEAT)
        return resp and resp['status'] == RESP_OK

    def unregister(self):
        if self.registered:
            self._send_request(REQ_UNREGISTER)
            self.registered = False

    def close(self):
        if self.sock:
            self.sock.close()
            self.sock = None

    def attach_shared_memory(self, shm_id, size):
        """使用 sysv_ipc 附加共享内存"""
        try:
            self.shm = sysv_ipc.SharedMemory(shm_id)
            self.shm_id = shm_id
            return True
        except Exception as e:
            print(f"附加共享内存失败: {e}")
            return False

    def attach_semaphore(self, sem_id):
        """使用 sysv_ipc 附加信号量"""
        try:
            self.sem = sysv_ipc.Semaphore(sem_id)
            self.sem_id = sem_id
            return True
        except Exception as e:
            print(f"附加信号量失败: {e}")
            return False

    def lock(self):
        """P 操作 (等待)"""
        if self.sem:
            self.sem.acquire()

    def unlock(self):
        """V 操作 (释放)"""
        if self.sem:
            self.sem.release()

    def read_data(self):
        """从共享内存读取 SpectrumData"""
        if not self.shm:
            return None
        buf = self.shm.read(ctypes.sizeof(SpectrumData))
        return SpectrumData.from_buffer_copy(buf)

    def write_data(self, data):
        """将 SpectrumData 写入共享内存"""
        if not self.shm:
            return False
        self.shm.write(bytes(data))
        return True


# ================== 心跳线程 ==================
class HeartbeatThread(QThread):
    def __init__(self, daemon_client):
        super().__init__()
        self.daemon = daemon_client
        self.running = True

    def run(self):
        while self.running:
            self.msleep(HEARTBEAT_INTERVAL * 1000)
            if self.running and self.daemon.registered:
                if not self.daemon.send_heartbeat():
                    print("心跳发送失败")

    def stop(self):
        self.running = False
        self.wait()


# ================== 信号发射器 ==================
class SignalEmitter(QObject):
    data_sent = pyqtSignal(bool, str)
    error_occurred = pyqtSignal(str)
    status_updated = pyqtSignal(str)
    consumer_started = pyqtSignal(bool)


# ================== 主窗口 ==================
class SpectrumControlGUI(QMainWindow):
    def __init__(self):
        super().__init__()
        self.daemon = DaemonClient()
        self.signal_emitter = SignalEmitter()
        self.consumer_process = None
        self.heartbeat_thread = None
        self.last_start_freq_hz = None   # 上次成功发送的起始频率 (Hz)
        self.last_stop_freq_hz = None    # 上次成功发送的终止频率 (Hz)
        self.last_mode = None            # 上次成功发送的模式 (0或1)

        # 连接信号
        self.signal_emitter.data_sent.connect(self.on_data_sent)
        self.signal_emitter.error_occurred.connect(self.on_error)
        self.signal_emitter.status_updated.connect(self.on_status_updated)
        self.signal_emitter.consumer_started.connect(self.on_consumer_started)

        self.init_ui()
        # self.init_daemon_and_shm()

    def init_ui(self):
        """初始化用户界面"""
        self.setWindowTitle("频谱仪频率控制系统")
        self.setGeometry(100, 100, 650, 500)

        central_widget = QWidget()
        self.setCentralWidget(central_widget)
        main_layout = QVBoxLayout()
        central_widget.setLayout(main_layout)

        # 标题
        title_label = QLabel("频谱仪频率控制")
        title_label.setAlignment(Qt.AlignCenter)
        title_font = QFont("Arial", 16, QFont.Bold)
        title_label.setFont(title_font)
        main_layout.addWidget(title_label)

        # 系统状态组
        status_group = QGroupBox("系统状态")
        status_layout = QGridLayout()
        self.status_label = QLabel("系统状态: 未连接守护进程")
        self.status_label.setStyleSheet("color: red; font-weight: bold;")
        status_layout.addWidget(self.status_label, 0, 0, 1, 2)

        self.consumer_status_label = QLabel("频谱仪进程: 未运行")
        self.consumer_status_label.setStyleSheet("color: orange;")
        status_layout.addWidget(self.consumer_status_label, 1, 0, 1, 2)

        status_group.setLayout(status_layout)
        main_layout.addWidget(status_group)

        # 频率设置组
        freq_group = QGroupBox("频率设置 (MHz)")
        freq_layout = QGridLayout()

        freq_layout.addWidget(QLabel("起始频率:"), 0, 0)
        self.start_freq_input = QLineEdit("100.0")
        self.start_freq_input.setValidator(QDoubleValidator(0.0, 10000.0, 2))
        freq_layout.addWidget(self.start_freq_input, 0, 1)
        freq_layout.addWidget(QLabel("MHz"), 0, 2)

        freq_layout.addWidget(QLabel("终止频率:"), 1, 0)
        self.stop_freq_input = QLineEdit("200.0")
        self.stop_freq_input.setValidator(QDoubleValidator(0.0, 10000.0, 2))
        freq_layout.addWidget(self.stop_freq_input, 1, 1)
        freq_layout.addWidget(QLabel("MHz"), 1, 2)

        # 模式选择
        freq_layout.addWidget(QLabel("工作模式:"), 2, 0)
        self.mode_combo = QComboBox()
        self.mode_combo.addItem("功率谱", 0)
        self.mode_combo.addItem("IQ", 1)
        freq_layout.addWidget(self.mode_combo, 2, 1)

        freq_group.setLayout(freq_layout)
        main_layout.addWidget(freq_group)

        # 按钮布局
        button_layout = QHBoxLayout()

        self.init_button = QPushButton("初始化系统")
        self.init_button.clicked.connect(self.initialize_system)
        button_layout.addWidget(self.init_button)

        self.send_button = QPushButton("发送设置")
        self.send_button.clicked.connect(self.send_frequency_settings)
        self.send_button.setEnabled(False)
        button_layout.addWidget(self.send_button)

        self.start_consumer_button = QPushButton("启动频谱仪")
        self.start_consumer_button.clicked.connect(self.start_consumer)
        button_layout.addWidget(self.start_consumer_button)

        self.clear_button = QPushButton("清除")
        self.clear_button.clicked.connect(self.clear_inputs)
        button_layout.addWidget(self.clear_button)

        self.exit_button = QPushButton("退出")
        self.exit_button.clicked.connect(self.close)
        button_layout.addWidget(self.exit_button)

        main_layout.addLayout(button_layout)

        # 日志区域
        log_group = QGroupBox("操作日志")
        log_layout = QVBoxLayout()
        self.log_text = QTextEdit()
        self.log_text.setReadOnly(True)
        self.log_text.setMaximumHeight(150)
        log_layout.addWidget(self.log_text)
        log_group.setLayout(log_layout)
        main_layout.addWidget(log_group)

        # 状态栏
        self.status_bar = QStatusBar()
        self.setStatusBar(self.status_bar)
        self.status_bar.showMessage("启动中...")

        main_layout.addStretch()

    def init_daemon_and_shm(self):
        """连接守护进程，注册，获取共享内存和信号量"""
        self.log_message("正在连接守护进程...")
        if not self.daemon.connect():
            self.signal_emitter.error_occurred.emit("无法连接守护进程，请确保 ipc_mgr 已启动")
            return
        self.log_message("连接成功，正在注册...")
        if not self.daemon.register():
            self.signal_emitter.error_occurred.emit("注册失败，守护进程可能未运行")
            return
        self.log_message("注册成功，获取共享内存信息...")
        info = self.daemon.get_shm_info()
        if not info:
            self.signal_emitter.error_occurred.emit("获取共享内存信息失败")
            return
        self.log_message(f"共享内存 ID: {info['shm_key']}, 信号量 ID: {info['sem_key']}")
        if not self.daemon.attach_shared_memory(info['shm_key'], info['shm_size']):
            self.signal_emitter.error_occurred.emit("附加共享内存失败")
            return
        if not self.daemon.attach_semaphore(info['sem_key']):
            self.signal_emitter.error_occurred.emit("附加信号量失败")
            return
        self.log_message("共享内存和信号量附加成功")
        self.status_label.setText("系统状态: 已初始化")
        self.status_label.setStyleSheet("color: green; font-weight: bold;")
        self.send_button.setEnabled(True)
        self.status_bar.showMessage("就绪")

        # 启动心跳线程
        self.heartbeat_thread = HeartbeatThread(self.daemon)
        self.heartbeat_thread.start()

    def initialize_system(self):
        """手动初始化（如果自动失败可重试）"""
        self.init_daemon_and_shm()
    def send_frequency_settings(self):
        """将频率和模式写入共享内存，并通知频谱仪（仅在参数变化时执行）"""
        try:
            start_mhz = float(self.start_freq_input.text())
            stop_mhz = float(self.stop_freq_input.text())
            if start_mhz <= 0 or stop_mhz <= 0:
                self.signal_emitter.error_occurred.emit("频率必须为正数")
                return
            if start_mhz >= stop_mhz:
                self.signal_emitter.error_occurred.emit("起始频率必须小于终止频率")
                return

            start_hz = start_mhz * 1e6
            stop_hz = stop_mhz * 1e6
            mode = self.mode_combo.currentData()

            # ========== 检查参数是否发生变化 ==========
            changed = False
            if (self.last_start_freq_hz is None or self.last_stop_freq_hz is None or self.last_mode is None):
                # 首次发送，视为有变化
                changed = True
            else:
                if (abs(start_hz - self.last_start_freq_hz) > 1e-3 or
                    abs(stop_hz - self.last_stop_freq_hz) > 1e-3):
                    changed = True
                elif mode != self.last_mode:
                    changed = True

            if not changed:
                self.log_message("参数未变化，无需发送")
                self.status_bar.showMessage("参数与上次相同，已忽略")
                return

            # ========== 参数有变化，执行写入和发送 ==========
            # 锁定信号量
            self.daemon.lock()

            # 读取当前数据（用于自增 sequence 和 ref_count）
            curr_data = self.daemon.read_data()
            if curr_data is None:
                curr_data = SpectrumData()
            # 更新字段
            curr_data.start_freq = start_hz
            curr_data.stop_freq = stop_hz
            curr_data.mode = mode
            curr_data.controller_pid = os.getpid()
            curr_data.sequence += 1
            curr_data.timestamp = int(time.time())
            curr_data.data_ready = 1
            curr_data.ref_count += 1   # 只有变化时才增加引用计数

            # 写入共享内存
            self.daemon.write_data(curr_data)

            # 解锁
            self.daemon.unlock()

            # 更新本地缓存（写入成功后再更新）
            self.last_start_freq_hz = start_hz
            self.last_stop_freq_hz = stop_hz
            self.last_mode = mode

            self.log_message(f"写入共享内存: {start_mhz:.2f}-{stop_mhz:.2f} MHz, 模式={mode}")
            self.signal_emitter.data_sent.emit(True, "设置已写入共享内存")

            # 通知频谱仪进程（只发送 SIGUSR1）
            if self.consumer_process and self.consumer_process.poll() is None:
                # 获取共享内存中的 spectrum_pid
                self.daemon.lock()
                updated = self.daemon.read_data()
                self.daemon.unlock()
                target_pid = updated.spectrum_pid if updated and updated.spectrum_pid != 0 else None
                if target_pid and target_pid > 0:
                    try:
                        os.kill(target_pid, signal.SIGUSR1)
                        self.log_message(f"已向频谱仪进程 {target_pid} 发送 SIGUSR1")
                    except ProcessLookupError:
                        self.log_message("频谱仪进程不存在，请重新启动")
                else:
                    if self.consumer_process:
                        try:
                            os.kill(self.consumer_process.pid, signal.SIGUSR1)
                            self.log_message(f"已向频谱仪进程 {self.consumer_process.pid} 发送 SIGUSR1")
                        except:
                            pass
            else:
                self.log_message("频谱仪未运行，仅更新共享内存")

        except ValueError:
            self.signal_emitter.error_occurred.emit("请输入有效的数字")
        except Exception as e:
            self.signal_emitter.error_occurred.emit(f"发送失败: {str(e)}")

    # def send_frequency_settings(self):
    #     """将频率和模式写入共享内存，并通知频谱仪"""
    #     try:
    #         start_mhz = float(self.start_freq_input.text())
    #         stop_mhz = float(self.stop_freq_input.text())
    #         if start_mhz <= 0 or stop_mhz <= 0:
    #             self.signal_emitter.error_occurred.emit("频率必须为正数")
    #             return
    #         if start_mhz >= stop_mhz:
    #             self.signal_emitter.error_occurred.emit("起始频率必须小于终止频率")
    #             return

    #         start_hz = start_mhz * 1e6
    #         stop_hz = stop_mhz * 1e6
    #         mode = self.mode_combo.currentData()

    #         # 锁定信号量
    #         self.daemon.lock()

    #         # 读取当前数据
    #         curr_data = self.daemon.read_data()
    #         if curr_data is None:
    #             # 如果读不到，创建新结构
    #             curr_data = SpectrumData()
    #         # 更新字段
    #         curr_data.start_freq = start_hz
    #         curr_data.stop_freq = stop_hz
    #         curr_data.mode = mode
    #         curr_data.controller_pid = os.getpid()
    #         curr_data.sequence += 1
    #         curr_data.timestamp = int(time.time())
    #         curr_data.data_ready = 1
    #         # ref_count 由守护进程或频谱仪维护，前端不修改？但根据您的说明，ref_count 是引用计数，前端应该自增。
    #         curr_data.ref_count += 1

    #         # 写入共享内存
    #         self.daemon.write_data(curr_data)

    #         # 解锁
    #         self.daemon.unlock()

    #         self.log_message(f"写入共享内存: {start_mhz:.2f}-{stop_mhz:.2f} MHz, 模式={mode}")
    #         self.signal_emitter.data_sent.emit(True, "设置已写入共享内存")

    #         # 通知频谱仪进程（如果已运行）
    #         if self.consumer_process and self.consumer_process.poll() is None:
    #             # 先获取共享内存中的 spectrum_pid，优先使用
    #             # 再次锁定读取
    #             self.daemon.lock()
    #             updated = self.daemon.read_data()
    #             self.daemon.unlock()
    #             target_pid = updated.spectrum_pid if updated and updated.spectrum_pid != 0 else None
    #             if target_pid and target_pid > 0:
    #                 try:
    #                     os.kill(target_pid, signal.SIGUSR1)  # 频率改变
    #                     self.log_message(f"已向频谱仪进程 {target_pid} 发送 SIGUSR1")
    #                 except ProcessLookupError:
    #                     self.log_message("频谱仪进程不存在，请重新启动")
    #             else:
    #                 # 若没有 spectrum_pid，尝试使用子进程 PID
    #                 if self.consumer_process:
    #                     try:
    #                         os.kill(self.consumer_process.pid, signal.SIGUSR1)
    #                         self.log_message(f"已向频谱仪进程 {self.consumer_process.pid} 发送 SIGUSR1")
    #                     except:
    #                         pass
    #         else:
    #             self.log_message("频谱仪未运行，仅更新共享内存")

    #     except ValueError:
    #         self.signal_emitter.error_occurred.emit("请输入有效的数字")
    #     except Exception as e:
    #         self.signal_emitter.error_occurred.emit(f"发送失败: {str(e)}")

    def start_consumer(self):
        """启动频谱仪消费者进程"""
        self.log_message("正在启动频谱仪进程...")
        self.consumer_status_label.setText("频谱仪进程: 正在启动...")
        self.consumer_status_label.setStyleSheet("color: blue;")

        try:
            self.consumer_process = subprocess.Popen(["./build/spectrum"])
            time.sleep(1)  # 等待进程启动并写入其 PID

            # 检查是否正常运行
            if self.consumer_process.poll() is None:
                self.consumer_status_label.setText(f"频谱仪进程: 运行中 (PID: {self.consumer_process.pid})")
                self.consumer_status_label.setStyleSheet("color: green;")
                self.log_message("频谱仪进程启动成功")
                self.signal_emitter.consumer_started.emit(True)

                # 等待 consumer 将自己的 PID 写入共享内存（轮询等待）
                for _ in range(10):
                    time.sleep(0.2)
                    self.daemon.lock()
                    data = self.daemon.read_data()
                    self.daemon.unlock()
                    if data and data.spectrum_pid != 0:
                        self.log_message(f"频谱仪 PID {data.spectrum_pid} 已记录到共享内存")
                        break
            else:
                self.signal_emitter.error_occurred.emit("频谱仪进程启动失败")
                self.consumer_status_label.setText("频谱仪进程: 失败")
                self.consumer_status_label.setStyleSheet("color: red;")

        except Exception as e:
            self.signal_emitter.error_occurred.emit(f"启动频谱仪异常: {str(e)}")
            self.consumer_status_label.setText("频谱仪进程: 错误")
            self.consumer_status_label.setStyleSheet("color: red;")

    def clear_inputs(self):
        self.start_freq_input.clear()
        self.stop_freq_input.clear()
        self.status_bar.showMessage("输入已清除")

    def log_message(self, message):
        timestamp = time.strftime("%H:%M:%S")
        self.log_text.append(f"[{timestamp}] {message}")

    def on_data_sent(self, success, message):
        if success:
            self.status_bar.showMessage(message)
        else:
            self.status_bar.showMessage(f"错误: {message}")

    def on_error(self, error_message):
        QMessageBox.critical(self, "错误", error_message)
        self.log_message(f"✗ {error_message}")

    def on_status_updated(self, status_message):
        self.status_bar.showMessage(status_message)

    def on_consumer_started(self, success):
        if success:
            self.log_message("✓ 频谱仪已就绪，可以发送设置")

    def closeEvent(self, event):
        reply = QMessageBox.question(
            self, "确认退出",
            "确定要退出频谱仪控制系统吗？",
            QMessageBox.Yes | QMessageBox.No,
            QMessageBox.No
        )
        if reply == QMessageBox.Yes:
            self.log_message("正在关闭系统...")

            # 停止心跳线程
            if self.heartbeat_thread:
                self.heartbeat_thread.stop()

            # 注销守护进程客户端
            self.daemon.unregister()
            self.daemon.close()

            # 终止频谱仪进程
            if self.consumer_process and self.consumer_process.poll() is None:
                try:
                    # 发送 SIGTERM 让频谱仪自行清理
                    self.consumer_process.terminate()
                    self.consumer_process.wait(timeout=2)
                    self.log_message("频谱仪进程已终止")
                except:
                    self.consumer_process.kill()

            self.log_message("系统资源已清理")
            event.accept()
        else:
            event.ignore()


def main():
    app = QApplication(sys.argv)
    app.setStyle("Fusion")
    window = SpectrumControlGUI()
    window.show()
    window.log_message("频谱仪控制系统已启动")
    window.log_message("请确保 ipc_mgr 守护进程已运行")
    sys.exit(app.exec_())


if __name__ == "__main__":
    main()