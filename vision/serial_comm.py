"""
串口通信模块
- 真实串口接收 (与下位机STM32通信)
- 模拟串口 (无硬件时使用)
- 协议：帧头0x66 + 数据 + 校验0x77
"""
import time
import threading
import numpy as np

try:
    import serial
    SERIAL_AVAILABLE = True
except ImportError:
    SERIAL_AVAILABLE = False


# 串口协议帧头
FRAME_HEADER = 0x66
FRAME_TAIL = 0x77

# 工作模式
MODE_IDLE = 0
MODE_COLOR = 1
MODE_RING = 3
MODE_DOCK = 4
MODE_QR = 9

# 命令字节
CMD_COLOR = 0x01   # 三色物料中心点
CMD_RING = 0x03    # 色环定位
CMD_DOCK = 0x04    # 码垛定位
CMD_QR = 0x09      # 二维码数据


class SerialComm:
    """
    串口通信类
    支持真实硬件串口 (CH340) 和无硬件模拟模式
    """
    def __init__(self, mock=True, port='/dev/ttyCH341USB0', baudrate=115200):
        """
        :param mock: True=模拟串口 False=真实硬件
        :param port: 串口设备路径 (Linux: /dev/ttyCH341USB0, Windows: COM3)
        :param baudrate: 波特率
        """
        self.mock = mock
        self.port = port
        self.baudrate = baudrate
        self.ser = None
        self.receive = [0, 0, 0, 0]      # 串口接收缓存
        self.send = [FRAME_HEADER] + [0x00] * 14  # 发送数据包
        self.unit = MODE_IDLE            # 当前工作模式
        self.unit_target = 0             # 目标编号
        self._thread = None
        self._running = False

    def open(self):
        """打开串口 (真实模式)"""
        if not SERIAL_AVAILABLE:
            print("pyserial未安装，无法打开真实串口")
            return False
        try:
            self.ser = serial.Serial(
                port=self.port,
                baudrate=self.baudrate,
                timeout=0.05
            )
            print(f"串口已打开: {self.port} @ {self.baudrate}")
            return True
        except Exception as e:
            print(f"串口打开失败: {e}")
            return False

    def close(self):
        """关闭串口"""
        self._running = False
        if self.ser:
            self.ser.close()

    def start(self):
        """启动串口接收线程"""
        if self.mock:
            self._running = True
            self._thread = threading.Thread(target=self._process_mock)
            self._thread.daemon = True
            self._thread.start()
        else:
            if self.open():
                self._running = True
                self._thread = threading.Thread(target=self._process_real)
                self._thread.daemon = True
                self._thread.start()
            else:
                # 打开失败回退到模拟模式
                self.mock = True
                self._running = True
                self._thread = threading.Thread(target=self._process_mock)
                self._thread.daemon = True
                self._thread.start()

    def _process_real(self):
        """真实串口接收循环 (4字节一帧)"""
        while self._running:
            try:
                input_data = self.ser.read(4)
                com_input = list(input_data)
                if com_input:
                    self.receive = [int(b) for b in com_input[:4]]
                    # 帧头校验: 102 = 0x66
                    if self.receive[0] == 102:
                        self.unit = self.receive[1]
                        self.unit_target = self.receive[2]
            except Exception as e:
                print(f"串口读取异常: {e}")
                time.sleep(0.01)

    def _process_mock(self):
        """模拟串口接收循环 (无硬件时使用)"""
        while self._running:
            time.sleep(0.1)
            self.unit = np.random.choice([1, 3, 4, 9, 0])

    def send_coordinates(self, cmd, coords, y_diff=None):
        """
        发送坐标到下位机
        协议格式: [0x66, cmd, x_high, x_low, y_high, y_low, ... , 0x77]
        :param cmd: 命令字节 (0x01/0x03/0x04/0x09)
        :param coords: 坐标列表 [(x1,y1), (x2,y2), ...]
        :param y_diff: Y方向差值
        """
        self.send = [FRAME_HEADER] + [0x00] * 14
        self.send[1] = cmd

        if cmd == CMD_COLOR:
            # unit=1: 发送三色物料中心点
            if len(coords) >= 1:
                x, y = coords[0]
                self.send[2] = (x & 0xff00) >> 8
                self.send[3] = (x & 0xff)
                self.send[4] = (y & 0xff00) >> 8
                self.send[5] = (y & 0xff)

        elif cmd == CMD_RING or cmd == CMD_DOCK:
            # unit=3/4: 发送中间坐标 + Y差值
            if len(coords) >= 3:
                x2, y2 = coords[1]   # 中间色环/物料
                y1 = coords[0][1]    # 左侧Y
                y3 = coords[2][1]    # 右侧Y
                self.send[2] = (x2 & 0xff00) >> 8
                self.send[3] = (x2 & 0xff)
                self.send[4] = (y2 & 0xff00) >> 8
                self.send[5] = (y2 & 0xff)
                diff = y1 - y3
                self.send[6] = (diff & 0xff00) >> 8
                self.send[7] = (diff & 0xff)

        # 校验字节
        self.send[8] = FRAME_TAIL

        # 真实串口发送
        if not self.mock and self.ser:
            try:
                self.ser.write(bytearray(self.send))
            except Exception as e:
                print(f"串口发送失败: {e}")
        else:
            print(f"[模拟] 发送数据: {self.send}")

    def send_qr_data(self, qr_data):
        """
        发送二维码数据
        :param qr_data: 二维码字符串
        """
        self.send = [FRAME_HEADER] + [0x00] * 14
        self.send[1] = CMD_QR
        qr_bytes = qr_data.encode('utf-8')[:6]
        for i, b in enumerate(qr_bytes):
            self.send[2 + i] = b
        self.send[8] = FRAME_TAIL

        if not self.mock and self.ser:
            try:
                self.ser.write(bytearray(self.send))
            except Exception:
                pass
        else:
            print(f"[模拟] 发送QR数据: {qr_data}")
