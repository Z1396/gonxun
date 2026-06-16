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
    预测+更新两步融合，输出平滑后的坐标
    """
    def __init__(self, q=1e-5, r=1e-2):
        """
        :param q: 过程噪声协方差 (越小越信任模型预测)
        :param r: 观测噪声协方差 (越小越信任测量值)
        """
        self.q = q
        self.r = r
        self.x = np.array([[0], [0]], dtype=np.float32)  # 状态向量
        self.p = np.eye(2)     # 误差协方差矩阵
        self.k = np.zeros((2, 2))  # 卡尔曼增益
        self.initialized = False  # 是否已初始化

    def predict(self):
        """预测步骤：根据上一帧状态预估当前位置"""
        self.p = self.p + self.q

    def update(self, z):
        """
        更新步骤：传入观测值z，融合预测值得到平滑坐标
        :param z: 观测值矩阵 [[x], [y]]
        :return: 滤波后的坐标
        """
        self.k = self.p @ np.linalg.inv(self.p + self.r)
        self.x = self.x + self.k @ (z - self.x)
        self.p = (np.eye(2) - self.k) @ self.p
        return self.x

    def filter(self, z):
        """
        一步完成预测+更新 (便捷接口)
        :param z: 观测值矩阵 [[x], [y]] 或 (x, y) 元组
        :return: 滤波后的坐标
        """
        # 兼容 (x, y) 元组输入
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
