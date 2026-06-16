"""
摄像头管理模块
- 主摄像头 (物料/色环识别)
- 扫码摄像头 (二维码识别)
"""
import cv2


class CameraManager:
    """
    摄像头管理器
    支持双摄像头：主摄像头 + 扫码摄像头
    """
    def __init__(self, main_index=0, qr_index=2):
        """
        :param main_index: 主摄像头索引 (物料/色环识别)
        :param qr_index: 扫码摄像头索引 (二维码识别)
        """
        self.main_index = main_index
        self.qr_index = qr_index
        self.cap = None        # 主摄像头
        self.cap2 = None       # 扫码摄像头

    def open(self):
        """打开主摄像头和扫码摄像头"""
        self.cap = cv2.VideoCapture(self.main_index)
        if not self.cap.isOpened():
            print(f"警告：无法打开主摄像头 {self.main_index}")
        else:
            print(f"主摄像头已打开: {self.main_index}")

        self.cap2 = cv2.VideoCapture(self.qr_index)
        if not self.cap2.isOpened():
            print(f"警告：无法打开扫码摄像头 {self.qr_index}")
        else:
            print(f"扫码摄像头已打开: {self.qr_index}")

    def close(self):
        """关闭所有摄像头"""
        if self.cap:
            self.cap.release()
        if self.cap2:
            self.cap2.release()

    def read_main(self):
        """读取主摄像头帧 :return: (success, frame)"""
        if self.cap and self.cap.isOpened():
            return self.cap.read()
        return False, None

    def read_qr(self):
        """读取扫码摄像头帧 :return: (success, frame)"""
        if self.cap2 and self.cap2.isOpened():
            return self.cap2.read()
        return False, None
