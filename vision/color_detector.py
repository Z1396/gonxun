"""
颜色识别模块
负责6种物料颜色的HSV阈值定义与色块中心检测
按比赛规则：红1/黄2/蓝3/绿4/黑5/浅蓝6
"""
import cv2
import numpy as np


# ========== 6种颜色HSV阈值字典 (比赛规则颜色) ==========
COLOR_DIST = {
    'red': {
        'Lower1': np.array([156, 60, 60]),
        'Upper1': np.array([180, 255, 255]),
        'Lower2': np.array([0, 60, 60]),
        'Upper2': np.array([6, 255, 255])
    },
    'yellow': {
        'Lower': np.array([20, 100, 100]),
        'Upper': np.array([34, 255, 255])
    },
    'blue': {
        'Lower': np.array([100, 100, 45]),
        'Upper': np.array([124, 255, 255])
    },
    'green': {
        'Lower': np.array([38, 80, 45]),
        'Upper': np.array([90, 255, 255])
    },
    'black': {
        'Lower': np.array([0, 0, 0]),
        'Upper': np.array([180, 255, 45])
    },
    'light_blue': {
        'Lower': np.array([85, 80, 100]),
        'Upper': np.array([100, 255, 255])
    }
}

# 颜色编号映射 (比赛规则)
COLOR_ID_MAP = {
    1: ('red', '红色'),
    2: ('yellow', '黄色'),
    3: ('blue', '蓝色'),
    4: ('green', '绿色'),
    5: ('black', '黑色'),
    6: ('light_blue', '浅蓝')
}


def get_color_by_id(color_id):
    """根据比赛规则的颜色编号(1~6)获取英文key和中文名"""
    if color_id in COLOR_ID_MAP:
        return COLOR_ID_MAP[color_id]
    return (None, None)


class ColorDetector:
    """
    颜色检测器：识别6种比赛物料颜色，返回色块中心坐标
    """
    def __init__(self):
        self.color_dist = COLOR_DIST

    def detect(self, img, color, size_code=2000):
        """
        HSV色块识别函数
        :param img: 输入原始BGR图像帧
        :param color: 识别颜色 key, 如 'red'/'yellow'/'blue'
        :param size_code: 轮廓面积阈值，过滤微小噪点
        :return: (center_x, center_y) 色块中心坐标；无目标返回None
        """
        if img is None or color not in self.color_dist:
            return None

        gs_img = cv2.GaussianBlur(img, (5, 5), 0)
        hsv_img = cv2.cvtColor(gs_img, cv2.COLOR_BGR2HSV)
        erode_hsv = cv2.erode(hsv_img, None, iterations=2)

        # 红色需要双段HSV范围（H分布在0附近和156~180）
        if color == 'red':
            inRange_hsv1 = cv2.inRange(erode_hsv,
                                       self.color_dist[color]['Lower1'],
                                       self.color_dist[color]['Upper1'])
            inRange_hsv2 = cv2.inRange(erode_hsv,
                                       self.color_dist[color]['Lower2'],
                                       self.color_dist[color]['Upper2'])
            inRange_hsv = inRange_hsv1 + inRange_hsv2
        else:
            inRange_hsv = cv2.inRange(erode_hsv,
                                      self.color_dist[color]['Lower'],
                                      self.color_dist[color]['Upper'])

        cnts = cv2.findContours(inRange_hsv.copy(), cv2.RETR_EXTERNAL,
                                cv2.CHAIN_APPROX_SIMPLE)[-2]

        if len(cnts) > 0:
            c = max(cnts, key=cv2.contourArea)
            size = int(cv2.contourArea(c))

            if size > size_code:
                rect = cv2.minAreaRect(c)
                box = cv2.boxPoints(rect)
                cv2.drawContours(img, [np.int0(box)], -1, (0, 255, 255), 2)
                center_x, center_y = rect[0]
                return (int(center_x), int(center_y))
        return None

    def detect_three_colors(self, img, colors=('red', 'green', 'blue'), size_code=2000):
        """
        批量检测三种颜色 (用于三色物料/码垛定位)
        :param img: BGR图像
        :param colors: 颜色key三元组
        :param size_code: 面积阈值
        :return: 坐标三元组，检测不到对应颜色返回None
        """
        results = []
        for c in colors:
            pos = self.detect(img, c, size_code)
            results.append(pos)
        return tuple(results)
