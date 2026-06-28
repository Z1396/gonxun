#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
串口通信模块 (POSIX termios API)
用于机器人比赛中的上下位机通信
仅支持 Linux/macOS，Windows 请用 pyserial 版本

使用示例:
    from vision.serial_posix import SerialPOSIX

    ser = SerialPOSIX("/dev/ttyCH341USB0", baudrate=115200)
    ser.open()
    ser.write(b'\x66\x01\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x77')
    data = ser.read(4, timeout=1.0)
    ser.close()
"""

import os
import time
import errno
import select
import struct
import termios
import fcntl
import threading
import logging

logger = logging.getLogger(__name__)


# ============================================================
# CRC-16/MODBUS 校验（查表法）
# ============================================================

_CRC16_TABLE = [
    0x0000, 0xC0C1, 0xC181, 0x0140, 0xC301, 0x03C0, 0x0280, 0xC241,
    0xC601, 0x06C0, 0x0780, 0xC741, 0x0500, 0xC5C1, 0xC481, 0x0440,
    0xCC01, 0x0CC0, 0x0D80, 0xCD41, 0x0F00, 0xCFC1, 0xCE81, 0x0E40,
    0x0A00, 0xCAC1, 0xCB81, 0x0B40, 0xC901, 0x09C0, 0x0880, 0xC841,
    0xD801, 0x18C0, 0x1980, 0xD941, 0x1B00, 0xDBC1, 0xDA81, 0x1A40,
    0x1E00, 0xDEC1, 0xDF81, 0x1F40, 0xDD01, 0x1DC0, 0x1C80, 0xDC41,
    0x1400, 0xD4C1, 0xD581, 0x1540, 0xD701, 0x17C0, 0x1680, 0xD641,
    0xD201, 0x12C0, 0x1380, 0xD341, 0x1100, 0xD1C1, 0xD081, 0x1040,
    0xF001, 0x30C0, 0x3180, 0xF141, 0x3300, 0xF3C1, 0xF281, 0x3240,
    0x3600, 0xF6C1, 0xF781, 0x3740, 0xF501, 0x35C0, 0x3480, 0xF441,
    0x3C00, 0xFCC1, 0xFD81, 0x3D40, 0xFF01, 0x3FC0, 0x3E80, 0xFE41,
    0xFA01, 0x3AC0, 0x3B80, 0xFB41, 0x3900, 0xF9C1, 0xF881, 0x3840,
    0x2800, 0xE8C1, 0xE981, 0x2940, 0xEB01, 0x2BC0, 0x2A80, 0xEA41,
    0xEE01, 0x2EC0, 0x2F80, 0xEF41, 0x2D00, 0xEDC1, 0xEC81, 0x2C40,
    0xE401, 0x24C0, 0x2580, 0xE541, 0x2700, 0xE7C1, 0xE681, 0x2640,
    0x2200, 0xE2C1, 0xE381, 0x2340, 0xE101, 0x21C0, 0x2080, 0xE041,
    0xA001, 0x60C0, 0x6180, 0xA141, 0x6300, 0xA3C1, 0xA281, 0x6240,
    0x6600, 0xA6C1, 0xA781, 0x6740, 0xA501, 0x65C0, 0x6480, 0xA441,
    0x6C00, 0xACC1, 0xAD81, 0x6D40, 0xAF01, 0x6FC0, 0x6E80, 0xAE41,
    0xAA01, 0x6AC0, 0x6B80, 0xAB41, 0x6900, 0xA9C1, 0xA881, 0x6840,
    0x7800, 0xB8C1, 0xB981, 0x7940, 0xBB01, 0x7BC0, 0x7A80, 0xBA41,
    0xBE01, 0x7EC0, 0x7F80, 0xBF41, 0x7D00, 0xBDC1, 0xBC81, 0x7C40,
    0xB401, 0x74C0, 0x7580, 0xB541, 0x7700, 0xB7C1, 0xB681, 0x7640,
    0x7200, 0xB2C1, 0xB381, 0x7340, 0xB101, 0x71C0, 0x7080, 0xB041,
    0x5000, 0x90C1, 0x9181, 0x5140, 0x9301, 0x53C0, 0x5280, 0x9241,
    0x9601, 0x56C0, 0x5780, 0x9741, 0x5500, 0x95C1, 0x9481, 0x5440,
    0x9C01, 0x5CC0, 0x5D80, 0x9D41, 0x5F00, 0x9FC1, 0x9E81, 0x5E40,
    0x5A00, 0x9AC1, 0x9B81, 0x5B40, 0x9901, 0x59C0, 0x5880, 0x9841,
    0x8801, 0x48C0, 0x4980, 0x8941, 0x4B00, 0x8BC1, 0x8A81, 0x4A40,
    0x4E00, 0x8EC1, 0x8F81, 0x4F40, 0x8D01, 0x4DC0, 0x4C80, 0x8C41,
    0x4400, 0x84C1, 0x8581, 0x4540, 0x8701, 0x47C0, 0x4680, 0x8641,
    0x8201, 0x42C0, 0x4380, 0x8341, 0x4100, 0x81C1, 0x8081, 0x4040,
]


def crc16(data: bytes) -> int:
    """计算 CRC-16/MODBUS 校验值"""
    crc = 0xFFFF
    for b in data:
        crc = (crc >> 8) ^ _CRC16_TABLE[(crc ^ b) & 0xFF]
    return crc & 0xFFFF


def crc16_append(data: bytes) -> bytes:
    """在数据末尾追加 CRC-16 (低字节在前)"""
    c = crc16(data)
    return data + struct.pack('<H', c)


def crc16_verify(data: bytes, expected_crc: int) -> bool:
    """验证 CRC-16 校验值"""
    return crc16(data) == expected_crc


class SerialPOSIX:
    """基于 termios 的串口通信，适合机器人比赛"""

    def __init__(self, port, baudrate=115200, timeout=1.0):
        self.port = port
        self.baudrate = baudrate
        self.timeout = timeout
        self._fd = -1
        self._is_open = False
        self._old_termios = None
        self._lock = threading.Lock()

        # 波特率映射
        self._baud_map = {
            9600: termios.B9600, 19200: termios.B19200,
            38400: termios.B38400, 57600: termios.B57600,
            115200: termios.B115200, 230400: termios.B230400,
            460800: termios.B460800, 921600: termios.B921600,
        }

    def open(self):
        """打开串口"""
        if self._is_open:
            return

        if not os.path.exists(self.port):
            raise OSError(f"设备不存在: {self.port}")

        self._fd = os.open(self.port, os.O_RDWR | os.O_NOCTTY | os.O_NDELAY)

        # 设为阻塞模式
        flags = fcntl.fcntl(self._fd, fcntl.F_GETFL, 0)
        fcntl.fcntl(self._fd, fcntl.F_SETFL, flags & ~os.O_NONBLOCK)

        # 配置 termios
        attr = termios.tcgetattr(self._fd)
        self._old_termios = list(attr)  # 保存原始配置

        iflag, oflag, cflag, lflag, ispeed, ospeed, cc = attr

        # 原始模式
        iflag &= ~(termios.IGNBRK | termios.BRKINT | termios.PARMRK |
                    termios.ISTRIP | termios.INLCR | termios.IGNCR |
                    termios.ICRNL | termios.IXON)
        oflag &= ~termios.OPOST
        lflag &= ~(termios.ECHO | termios.ECHONL | termios.ICANON |
                    termios.ISIG | termios.IEXTEN)

        # 8N1: 8数据位, 无校验, 1停止位
        cflag &= ~(termios.CSIZE | termios.PARENB | termios.CSTOPB)
        cflag |= termios.CS8 | termios.CREAD | termios.CLOCAL

        # 波特率
        baud = self._baud_map.get(self.baudrate)
        if baud is None:
            os.close(self._fd)
            raise ValueError(f"不支持的波特率: {self.baudrate}")
        ispeed = baud
        ospeed = baud

        # 超时: 100ms
        cc[termios.VMIN] = 0
        cc[termios.VTIME] = 1

        termios.tcsetattr(self._fd, termios.TCSANOW,
                          [iflag, oflag, cflag, lflag, ispeed, ospeed, cc])
        termios.tcflush(self._fd, termios.TCIOFLUSH)

        self._is_open = True
        logger.info(f"串口已打开: {self.port} @ {self.baudrate}")

    def close(self):
        """关闭串口，恢复原始配置"""
        if not self._is_open:
            return

        if self._old_termios is not None:
            try:
                termios.tcsetattr(self._fd, termios.TCSANOW, self._old_termios)
            except OSError:
                pass

        try:
            os.close(self._fd)
        except OSError:
            pass

        self._fd = -1
        self._is_open = False
        self._old_termios = None
        logger.info(f"串口已关闭: {self.port}")

    def write(self, data: bytes) -> int:
        """写入数据"""
        if not self._is_open:
            raise OSError("串口未打开")
        with self._lock:
            return os.write(self._fd, data)

    def write_frame(self, data: bytes) -> int:
        """写入带 CRC-16 校验的数据帧（数据 + 2字节CRC）"""
        return self.write(crc16_append(data))

    def read(self, size: int, timeout: float = None) -> bytes:
        """读取数据，支持超时"""
        if not self._is_open:
            raise OSError("串口未打开")

        if timeout is None:
            timeout = self.timeout

        buf = bytearray()
        deadline = time.monotonic() + timeout

        while len(buf) < size:
            remaining = deadline - time.monotonic()
            if remaining <= 0:
                break

            try:
                rlist, _, _ = select.select([self._fd], [], [], remaining)
                if not rlist:
                    break
                chunk = os.read(self._fd, size - len(buf))
                if not chunk:
                    break
                buf.extend(chunk)
            except OSError as e:
                if e.errno == errno.EINTR:
                    continue
                raise

        return bytes(buf)

    def read_frame(self, size: int, timeout: float = None) -> tuple:
        """
        读取带 CRC-16 校验的数据帧
        :return: (data, ok)  ok=False 表示 CRC 校验失败
        """
        frame = self.read(size + 2, timeout)
        if len(frame) < size + 2:
            return frame, False
        payload = frame[:size]
        received_crc = struct.unpack('<H', frame[size:])[0]
        ok = crc16_verify(payload, received_crc)
        return payload, ok

    def flush(self):
        """清空缓冲区"""
        if self._is_open:
            try:
                termios.tcflush(self._fd, termios.TCIOFLUSH)
            except OSError:
                pass

    def __enter__(self):
        self.open()
        return self

    def __exit__(self, *args):
        self.close()

    def __repr__(self):
        return f"SerialPOSIX(port='{self.port}', baudrate={self.baudrate}, open={self._is_open})"
