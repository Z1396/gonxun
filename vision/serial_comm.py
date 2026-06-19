"""
串口通信模块
- 真实串口接收 (与下位机STM32通信)
- 模拟串口 (无硬件时使用)
- 协议：帧头0x66 + 命令 + 数据 + 校验和 + 帧尾0x77
"""
import time
import threading

try:
    import serial
    SERIAL_AVAILABLE = True
except ImportError:
    SERIAL_AVAILABLE = False

# 串口协议常量
FRAME_HEADER = 0x66
FRAME_TAIL = 0x77
FRAME_DATA_LEN = 12
FRAME_CHECKSUM_IDX = 13
FRAME_TAIL_IDX = 14

# 工作模式
MODE_IDLE = 0
MODE_COLOR = 1
MODE_RING = 3
MODE_DOCK = 4
MODE_QR = 9

# 命令字节
CMD_COLOR = 0x01
CMD_RING = 0x03
CMD_DOCK = 0x04
CMD_QR = 0x09


def _pack_word(value):
    """将16位有符号整数拆分为高8位、低8位字节元组"""
    return (value & 0xff00) >> 8, value & 0xff


class SerialComm:
    """串口通信类，支持真实硬件串口和无硬件模拟模式"""

    def __init__(self, mock=True, port='/dev/ttyCH341USB0', baudrate=115200,
                 mock_unit=MODE_IDLE, mock_cycle=False):
        self.mock = mock
        self.port = port
        self.baudrate = baudrate
        self.mock_unit = mock_unit
        self.mock_cycle = mock_cycle
        self.ser = None
        self.receive = [0, 0, 0, 0]
        self.send = [FRAME_HEADER] + [0x00] * 14
        self.unit = MODE_IDLE
        self.unit_target = 0
        self._thread = None
        self._running = False
        self._mock_cycle_units = [MODE_COLOR, MODE_RING, MODE_DOCK, MODE_QR, MODE_IDLE]
        self._mock_cycle_idx = 0

    def open(self):
        """打开串口（真实模式）"""
        if not SERIAL_AVAILABLE:
            print("pyserial未安装，无法打开真实串口")
            return False
        try:
            self.ser = serial.Serial(port=self.port, baudrate=self.baudrate, timeout=0.05)
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
            self._start_thread(self._process_mock)
            return

        if self.open():
            self._start_thread(self._process_real)
        else:
            print("[警告] 真实串口打开失败，回退到模拟模式")
            self.mock = True
            self._start_thread(self._process_mock)

    def _start_thread(self, target):
        """启动后台线程"""
        self._running = True
        self._thread = threading.Thread(target=target)
        self._thread.daemon = True
        self._thread.start()

    def _process_real(self):
        """真实串口接收循环"""
        while self._running:
            try:
                input_data = self.ser.read(4)
                com_input = list(input_data)
                if com_input:
                    self.receive = [int(b) for b in com_input[:4]]
                    if self.receive[0] == 102:  # 帧头 0x66
                        self.unit = self.receive[1]
                        self.unit_target = self.receive[2]
            except Exception as e:
                print(f"串口读取异常: {e}")
                time.sleep(0.01)

    def _process_mock(self):
        """模拟串口接收循环"""
        while self._running:
            time.sleep(0.1)
            if self.mock_cycle:
                self.unit = self._mock_cycle_units[self._mock_cycle_idx]
                self._mock_cycle_idx = (self._mock_cycle_idx + 1) % len(self._mock_cycle_units)
            else:
                self.unit = self.mock_unit

    def _build_frame(self, cmd, data_bytes):
        """构建发送帧：帧头 + 命令 + 数据(12字节) + 校验和 + 帧尾"""
        self.send = [FRAME_HEADER] + [0x00] * 14
        self.send[1] = cmd
        for i, b in enumerate(data_bytes[:FRAME_DATA_LEN]):
            self.send[2 + i] = b
        self.send[FRAME_CHECKSUM_IDX] = sum(self.send[1:FRAME_CHECKSUM_IDX]) & 0xFF
        self.send[FRAME_TAIL_IDX] = FRAME_TAIL
        return self.send

    def send_coordinates(self, cmd, coords, y_diff=None):
        """
        发送坐标到下位机

        :param cmd: 命令字节 (0x01/0x03/0x04)
        :param coords: 坐标列表 [(x1,y1), (x2,y2), ...]
        """
        data = [0x00] * FRAME_DATA_LEN

        if cmd == CMD_COLOR:
            # 三色物料中心点 (每个坐标2字节x+2字节y，共12字节)
            if len(coords) >= 3:
                for i in range(3):
                    x, y = coords[i]
                    data[i * 4 + 0], data[i * 4 + 1] = _pack_word(x)
                    data[i * 4 + 2], data[i * 4 + 3] = _pack_word(y)

        elif cmd == CMD_RING or cmd == CMD_DOCK:
            # 中间坐标 + Y差值
            if len(coords) >= 3:
                x2, y2 = coords[1]
                y1 = coords[0][1]
                y3 = coords[2][1]
                data[0], data[1] = _pack_word(x2)
                data[2], data[3] = _pack_word(y2)
                data[4], data[5] = _pack_word(y1 - y3)

        self._build_frame(cmd, data)
        self._transmit()

    def send_qr_data(self, qr_data):
        """发送二维码数据"""
        data = [0x00] * FRAME_DATA_LEN
        qr_bytes = qr_data.encode('utf-8')[:FRAME_DATA_LEN]
        for i, b in enumerate(qr_bytes):
            data[i] = b
        self._build_frame(CMD_QR, data)
        self._transmit()

    def _transmit(self):
        """真实串口/模拟串口发送"""
        if not self.mock and self.ser:
            try:
                self.ser.write(bytearray(self.send))
            except Exception as e:
                print(f"串口发送失败: {e}")
        else:
            print(f"[模拟] 发送数据: {self.send}")
