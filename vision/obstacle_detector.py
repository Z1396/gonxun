"""
障碍物检测模块
比赛规则：黑色模拟障碍物φ50×100mm，数量位置随机抽签放置
使用HSV低亮度+低饱和度掩码 + 霍夫圆检测
"""
import cv2
import numpy as np


class ObstacleDetector:
    """
    黑色障碍物检测器
    """
    def __init__(self, min_radius=15, max_radius=35):
        self.min_radius = min_radius
        self.max_radius = max_radius

    def detect(self, img):
        """
        检测黑色障碍物
        :param img: 输入BGR图像
        :return: 障碍物中心坐标列表 [(x,y,r), ...]
        """
        if img is None:
            return []
        # 黑色HSV: V通道低，S通道低
        hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)
        # 黑色掩码 (低饱和度、低亮度)
        lower_black = np.array([0, 0, 0])
        upper_black = np.array([180, 255, 50])
        mask = cv2.inRange(hsv, lower_black, upper_black)

        # 形态学去噪
        kernel = np.ones((5, 5), np.uint8)
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, kernel)
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, kernel)

        # 霍夫圆检测
        circles = cv2.HoughCircles(
            mask, cv2.HOUGH_GRADIENT, dp=1, minDist=50,
            param1=50, param2=15,
            minRadius=self.min_radius, maxRadius=self.max_radius
        )
        obstacles = []
        if circles is not None:
            for c in circles[0]:
                cx, cy, r = int(c[0]), int(c[1]), int(c[2])
                obstacles.append((cx, cy, r))
                cv2.circle(img, (cx, cy), r, (0, 0, 255), 2)
                cv2.circle(img, (cx, cy), 2, (0, 0, 255), -1)
        return obstacles
