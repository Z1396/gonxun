"""
障碍物检测模块
比赛规则：黑色模拟障碍物φ50×100mm，数量位置随机抽签放置
使用HSV低亮度+低饱和度掩码 + 霍夫圆检测
"""
import cv2
import numpy as np


class ObstacleDetector:
    """黑色障碍物检测器"""

    _LOWER_BLACK = np.array([0, 0, 0])
    _UPPER_BLACK = np.array([180, 80, 60])
    _MORPH_KERNEL = np.ones((5, 5), np.uint8)

    _HOUGH_DP = 1
    _HOUGH_MINDIST = 50
    _HOUGH_PARAM1 = 50
    _HOUGH_PARAM2 = 15

    _CIRCLE_COLOR = (0, 0, 255)
    _CENTER_COLOR = (0, 255, 0)
    _THICKNESS = 2

    def __init__(self, min_radius=15, max_radius=35, min_area=500):
        self.min_radius = min_radius
        self.max_radius = max_radius
        self.min_area = min_area

    def detect(self, img):
        """
        检测黑色障碍物

        :return: 障碍物中心坐标列表 [(x,y,r), ...]
        """
        if img is None:
            return []
        hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)
        mask = cv2.inRange(hsv, self._LOWER_BLACK, self._UPPER_BLACK)

        # 形态学去噪
        mask = cv2.morphologyEx(mask, cv2.MORPH_OPEN, self._MORPH_KERNEL)
        mask = cv2.morphologyEx(mask, cv2.MORPH_CLOSE, self._MORPH_KERNEL)

        circles = cv2.HoughCircles(
            mask, cv2.HOUGH_GRADIENT,
            dp=self._HOUGH_DP, minDist=self._HOUGH_MINDIST,
            param1=self._HOUGH_PARAM1, param2=self._HOUGH_PARAM2,
            minRadius=self.min_radius, maxRadius=self.max_radius
        )
        obstacles = []
        if circles is not None:
            for c in circles[0]:
                cx, cy, r = int(c[0]), int(c[1]), int(c[2])
                area = np.pi * r * r
                if area >= self.min_area:
                    obstacles.append((cx, cy, r))
        return obstacles

    def detect_and_draw(self, img):
        """检测黑色障碍物并绘制标记"""
        obstacles = self.detect(img)
        for cx, cy, r in obstacles:
            cv2.circle(img, (cx, cy), r, self._CIRCLE_COLOR, self._THICKNESS)
            cv2.circle(img, (cx, cy), 2, self._CENTER_COLOR, -1)
        return obstacles
