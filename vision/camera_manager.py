"""
摄像头管理模块
- 主摄像头 (物料/色环识别)
- 扫码摄像头 (二维码识别)
使用线程锁保护 VideoCapture.read()，避免多线程下的帧数据竞争
"""
import threading
import cv2


class CameraManager:
    """双摄像头管理器：主摄像头 + 扫码摄像头"""

    def __init__(self, main_index=0, qr_index=2,
                 main_width=640, main_height=480,
                 qr_width=640, qr_height=480):
        self.main_index = main_index
        self.qr_index = qr_index
        self.main_resolution = (main_width, main_height)
        self.qr_resolution = (qr_width, qr_height)
        self.cap = None
        self.cap2 = None
        self._main_lock = threading.Lock()
        self._qr_lock = threading.Lock()

    def _configure_capture(self, cap, resolution):
        """配置摄像头分辨率与缓冲区大小"""
        if cap is None or not cap.isOpened():
            return
        width, height = resolution
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

    def _open_one(self, index, resolution, name):
        """打开单个摄像头并配置分辨率"""
        cap = cv2.VideoCapture(index)
        if not cap.isOpened():
            print(f"警告：无法打开{name}摄像头 {index}")
            return None
        self._configure_capture(cap, resolution)
        print(f"{name}摄像头已打开: {index}")
        return cap

    def open(self):
        """打开主摄像头和扫码摄像头"""
        self.cap = self._open_one(self.main_index, self.main_resolution, "主")
        self.cap2 = self._open_one(self.qr_index, self.qr_resolution, "扫码")

    def close(self):
        """关闭所有摄像头"""
        with self._main_lock:
            if self.cap:
                self.cap.release()
                self.cap = None
        with self._qr_lock:
            if self.cap2:
                self.cap2.release()
                self.cap2 = None

    def close_cameras(self):
        """close 的别名，兼容旧接口"""
        self.close()

    def read_main(self):
        """读取主摄像头帧"""
        with self._main_lock:
            if self.cap and self.cap.isOpened():
                return self.cap.read()
        return False, None

    def read_qr(self):
        """读取扫码摄像头帧"""
        with self._qr_lock:
            if self.cap2 and self.cap2.isOpened():
                return self.cap2.read()
        return False, None
