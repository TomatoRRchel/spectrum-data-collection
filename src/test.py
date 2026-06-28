#!/usr/bin/env python3
import sys
import threading
import time
from PyQt5.QtWidgets import QApplication, QLabel, QVBoxLayout, QWidget

def test_qt():
    print("尝试启动Qt...")
    app = QApplication(sys.argv)
    
    window = QWidget()
    window.setWindowTitle("Qt测试")
    layout = QVBoxLayout()
    label = QLabel("Qt启动成功！")
    layout.addWidget(label)
    window.setLayout(layout)
    window.show()
    
    print("Qt启动成功，窗口已显示")
    sys.exit(app.exec_())

# 设置超时
def timeout_kill():
    time.sleep(5)
    print("ERROR: Qt启动超时！")
    sys.exit(1)

if __name__ == "__main__":
    timer = threading.Thread(target=timeout_kill, daemon=True)
    timer.start()
    test_qt()