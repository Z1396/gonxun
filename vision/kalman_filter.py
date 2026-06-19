"""
卡尔曼滤波器模块
二维卡尔曼滤波，用于平滑图像识别得到的物体中心坐标，
消除摄像头抖动、识别跳变等噪声
"""
import numpy as np


class KalmanFilter:
    """
    二维卡尔曼滤波器
    状态量：[x, y] 目标中心点横纵坐标
    """

    def __init__(self, q=1e-5, r=1e-2):
        """
        :param q: 过程噪声协方差 (越小越信任模型预测)
        :param r: 观测噪声协方差 (越小越信任测量值)
        """
        self.q = q
        self.r = r
        self.x = np.array([[0], [0]], dtype=np.float32)
        self.p = np.eye(2)
        self.initialized = False

    def predict(self):
        """预测步骤"""
        self.p = self.p + self.q

    def update(self, z):
        """更新步骤：融合预测值与观测值"""
        # 2x2对角矩阵求逆优化：闭式解替代 np.linalg.inv
        p_diag = np.diag(self.p)
        pr = p_diag + self.r
        k_diag = pr / (pr + self.r)
        innovation = z - self.x
        self.x = self.x + k_diag.reshape(2, 1) * innovation
        self.p = np.diag((1 - k_diag) * p_diag)
        return self.x

    def filter(self, z):
        """一步完成预测+更新，支持 [[x],[y]] 矩阵或 (x,y) 元组"""
        if isinstance(z, (tuple, list)):
            z = np.array([[z[0]], [z[1]]], dtype=np.float32)
        self.predict()
        result = self.update(z)
        self.initialized = True
        return result

    def reset(self):
        """重置滤波器状态"""
        self.x = np.array([[0], [0]], dtype=np.float32)
        self.p = np.eye(2)
        self.initialized = False
