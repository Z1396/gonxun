"""
摄像头管理模块
- 主摄像头 (物料/色环识别)
- 扫码摄像头 (二维码识别)
使用线程锁保护 VideoCapture.read()，避免多线程下的帧数据竞争
支持自动重连、跨平台后端选择、降级分辨率
"""
import time
import platform
import threading
import cv2


# 跨平台摄像头后端
def _get_preferred_backend():
    """根据操作系统选择最优后端"""
    if platform.system() == 'Windows':
        return cv2.CAP_DSHOW  # Windows: DirectShow 更稳定
    elif platform.system() == 'Linux':
        return cv2.CAP_V4L2   # Linux: V4L2
    else:
        return cv2.CAP_ANY    # macOS/其他: 自动选择


# 降级分辨率列表（宽, 高）
FALLBACK_RESOLUTIONS = [
    (640, 480),
    (320, 240),
    (160, 120),
]


class CameraManager:
    """双摄像头管理器：主摄像头 + 扫码摄像头"""

    def __init__(self, main_index=1, qr_index=2,
                 main_width=640, main_height=480,
                 qr_width=640, qr_height=480,
                 max_reconnect=3, reconnect_delay=1.0):
        self.main_index = main_index
        self.qr_index = qr_index
        self.main_resolution = (main_width, main_height)
        self.qr_resolution = (qr_width, qr_height)
        self.max_reconnect = max_reconnect
        self.reconnect_delay = reconnect_delay
        self.backend = _get_preferred_backend()

        self.cap = None
        self.cap2 = None
        self._main_lock = threading.Lock()
        self._qr_lock = threading.Lock()

        # 连续读取失败计数
        self._main_fail_count = 0
        self._qr_fail_count = 0
        self._fail_threshold = 30  # 连续失败30帧触发重连

    def _configure_capture(self, cap, resolution):
        """配置摄像头分辨率与缓冲区大小"""
        if cap is None or not cap.isOpened():
            return
        width, height = resolution
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

    def _open_one(self, index, resolution, name, backend=None):
        """打开单个摄像头，支持多后端尝试和分辨率降级"""
        if backend is None:
            backend = self.backend

        # 尝试不同后端
        backends = [backend, cv2.CAP_ANY]
        for be in backends:
            cap = cv2.VideoCapture(index, be)
            if cap.isOpened():
                break
            cap.release()
            cap = None

        if cap is None:
            print(f"[摄像头] 警告：无法打开{name}摄像头 {index}")
            return None

        # 尝试设置分辨率，失败则降级
        width, height = resolution
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        cap.set(cv2.CAP_PROP_BUFFERSIZE, 1)

        # 验证实际分辨率
        actual_w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        actual_h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))

        if actual_w != width or actual_h != height:
            print(f"[摄像头] {name}摄像头分辨率降级: "
                  f"请求{width}x{height}, 实际{actual_w}x{actual_h}")

        print(f"[摄像头] {name}摄像头已打开: index={index}, "
              f"分辨率={actual_w}x{actual_h}, 后端={'DSHOW' if be == cv2.CAP_DSHOW else 'AUTO'}")
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
        self._main_fail_count = 0
        self._qr_fail_count = 0

    def close_cameras(self):
        """close 的别名，兼容旧接口"""
        self.close()

    def _reconnect(self, index, resolution, name, current_cap, lock):
        """重连摄像头"""
        with lock:
            if current_cap:
                current_cap.release()
            print(f"[摄像头] 正在重连{name}摄像头 {index}...")
            time.sleep(self.reconnect_delay)
            new_cap = self._open_one(index, resolution, name)
            if new_cap:
                print(f"[摄像头] {name}摄像头重连成功")
            else:
                print(f"[摄像头] {name}摄像头重连失败")
            return new_cap

    def read_main(self):
        """读取主摄像头帧，支持自动重连"""
        with self._main_lock:
            if self.cap and self.cap.isOpened():
                ret, frame = self.cap.read()
                if ret:
                    self._main_fail_count = 0
                    return True, frame
                else:
                    self._main_fail_count += 1
                    if self._main_fail_count >= self._fail_threshold:
                        self._main_fail_count = 0
                        # 释放锁后重连
                        self.cap.release()
                        self.cap = None

        # 尝试重连
        if self.cap is None:
            self.cap = self._reconnect(
                self.main_index, self.main_resolution, "主",
                self.cap, self._main_lock
            )
        return False, None

    def read_qr(self):
        """读取扫码摄像头帧，支持自动重连"""
        with self._qr_lock:
            if self.cap2 and self.cap2.isOpened():
                ret, frame = self.cap2.read()
                if ret:
                    self._qr_fail_count = 0
                    return True, frame
                else:
                    self._qr_fail_count += 1
                    if self._qr_fail_count >= self._fail_threshold:
                        self._qr_fail_count = 0
                        self.cap2.release()
                        self.cap2 = None

        if self.cap2 is None:
            self.cap2 = self._reconnect(
                self.qr_index, self.qr_resolution, "扫码",
                self.cap2, self._qr_lock
            )
        return False, None
