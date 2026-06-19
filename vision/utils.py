"""
视觉系统工具模块
- FPSCounter: 帧率计数器
- check_gui_available: 检测 OpenCV 图形窗口支持
- generate_test_frame: 生成测试画布（相机读取失败时使用）
"""
import cv2
import numpy as np
import time


class FPSCounter:
    """FPS 帧率计数器"""

    def __init__(self, update_interval=10):
        self.update_interval = update_interval
        self.fps = 0.0
        self._frame_count = 0
        self._start_time = time.time()

    def tick(self):
        """每帧调用，返回当前 FPS"""
        self._frame_count += 1
        if self._frame_count >= self.update_interval:
            elapsed = time.time() - self._start_time
            self.fps = self._frame_count / elapsed if elapsed > 0 else 0
            self._frame_count = 0
            self._start_time = time.time()
        return self.fps


def check_gui_available():
    """检测当前环境是否支持 OpenCV 图形窗口"""
    try:
        cv2.namedWindow('test')
        cv2.destroyWindow('test')
        return True
    except Exception:
        return False


def generate_test_frame():
    """生成测试画布（相机读取失败时使用）"""
    img = np.zeros((480, 640, 3), dtype=np.uint8)
    img[:] = (50, 50, 50)
    cv2.circle(img, (200, 200), 30, (0, 0, 255), -1)
    cv2.circle(img, (320, 200), 30, (0, 255, 0), -1)
    cv2.circle(img, (440, 200), 30, (255, 0, 0), -1)
    return img