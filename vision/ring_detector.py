"""
圆环检测模块
- 3色环定位 (底盘标定用)
- 6环识别 (粗加工区/暂存区，按X排序映射1~6)
- 6环评分表 (比赛规则表3)
"""
import cv2
import numpy as np


# 6环尺寸评分表 (比赛规则表3)
RING_SCORES = {1: 15, 2: 10, 3: 7, 4: 5, 5: 3, 6: 1}


def calc_placement_score(ring_id, material_fallen=False):
    """计算放置得分，物料倾倒或环号无效返回0"""
    if material_fallen or ring_id is None or ring_id < 1 or ring_id > 6:
        return 0
    return RING_SCORES.get(ring_id, 0)


class ThreeRingDetector:
    """三色定位环检测器：识别3个标记色环，按X排序"""

    _ERODE_ITER = 2
    _DILATE_KERNEL = np.ones((7, 7), np.uint8)
    _DILATE_ITER = 1
    _CLAHE_CLIP = 5.0
    _CLAHE_GRID = (8, 8)
    _GRAD_KERNEL = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
    _GAUSS_KSIZE = (7, 7)
    _GAUSS_SIGMA = 3
    _THRESH_VAL = 70
    _THRESH_GAUSS_KSIZE = (9, 9)
    _HOUGH_DP = 1.5
    _HOUGH_MINDIST = 50
    _HOUGH_PARAM1 = 100
    _HOUGH_PARAM2 = 0.95
    _HOUGH_MINR = 15
    _HOUGH_MAXR = 50

    def detect(self, img):
        """
        霍夫圆检测，识别三个定位色环标记

        :return: (x1,y1,x2,y2,x3,y3) 按X从左到右排序；检测不到3个圆返回None
        """
        if img is None:
            return None

        # 预处理：腐蚀 → 膨胀 → 灰度 → 直方图均衡 → 形态学梯度
        hsv = cv2.cvtColor(img, cv2.COLOR_BGR2HSV)
        erode_kernel = np.ones((3, 3), np.uint8)
        erode_hsv = cv2.erode(hsv, erode_kernel, iterations=self._ERODE_ITER)
        dilated = cv2.dilate(erode_hsv, self._DILATE_KERNEL, iterations=self._DILATE_ITER)
        gray = cv2.cvtColor(dilated, cv2.COLOR_BGR2GRAY)

        clahe = cv2.createCLAHE(clipLimit=self._CLAHE_CLIP, tileGridSize=self._CLAHE_GRID)
        equalized = clahe.apply(gray)
        gradient = cv2.morphologyEx(equalized, cv2.MORPH_GRADIENT, self._GRAD_KERNEL)

        # 高斯增强 + 阈值分割
        blurred = cv2.GaussianBlur(gradient, self._GAUSS_KSIZE, self._GAUSS_SIGMA, self._GAUSS_SIGMA)
        scaled = cv2.convertScaleAbs(blurred, alpha=4, beta=0)
        scaled = cv2.GaussianBlur(scaled, self._GAUSS_KSIZE, self._GAUSS_SIGMA, self._GAUSS_SIGMA)

        _, thresholded = cv2.threshold(scaled, self._THRESH_VAL, 255, cv2.THRESH_BINARY)
        thresholded = cv2.GaussianBlur(thresholded, self._THRESH_GAUSS_KSIZE,
                                       self._GAUSS_SIGMA, self._GAUSS_SIGMA)

        circles = cv2.HoughCircles(
            thresholded, cv2.HOUGH_GRADIENT_ALT,
            dp=self._HOUGH_DP, minDist=self._HOUGH_MINDIST,
            param1=self._HOUGH_PARAM1, param2=self._HOUGH_PARAM2,
            minRadius=self._HOUGH_MINR, maxRadius=self._HOUGH_MAXR
        )

        if circles is not None and len(circles[0]) == 3:
            circles = np.uint16(np.around(circles))
            for circle in circles[0, :]:
                cx, cy, r = circle
                cv2.circle(img, (cx, cy), r, (0, 0, 255), 2)
                cv2.circle(img, (cx, cy), 2, (255, 0, 0), 2)
            # 按X坐标排序 → 左/中/右
            sorted_pts = sorted((c[0], c[1]) for c in circles[0])
            return tuple(v for pt in sorted_pts for v in pt)
        return None


class SixRingDetector:
    """6环检测器：识别粗加工区/暂存区的6个圆环位置，按X坐标映射1~6号"""

    _BLUR_KSIZE = (5, 5)
    _CANNY_LOW = 50
    _CANNY_HIGH = 150
    _HOUGH_DP = 1
    _HOUGH_MINDIST = 40
    _HOUGH_PARAM1 = 100
    _HOUGH_PARAM2 = 30
    _HOUGH_MINR = 20
    _HOUGH_MAXR = 120
    _RING_COLOR = (255, 0, 255)
    _RING_THICKNESS = 2
    _LABEL_FONT = cv2.FONT_HERSHEY_SIMPLEX
    _LABEL_SCALE = 0.6

    def detect(self, img):
        """
        检测6个圆环位置

        :return: {ring_id: (x,y), ...} 检测失败返回None
        """
        if img is None:
            return None
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        blurred = cv2.GaussianBlur(gray, self._BLUR_KSIZE, 0)
        edges = cv2.Canny(blurred, self._CANNY_LOW, self._CANNY_HIGH)

        circles = cv2.HoughCircles(
            edges, cv2.HOUGH_GRADIENT,
            dp=self._HOUGH_DP, minDist=self._HOUGH_MINDIST,
            param1=self._HOUGH_PARAM1, param2=self._HOUGH_PARAM2,
            minRadius=self._HOUGH_MINR, maxRadius=self._HOUGH_MAXR
        )
        if circles is None:
            return None

        # 按X坐标排序，从左到右对应1~6号环
        detected = sorted([(int(c[0]), int(c[1]), int(c[2])) for c in circles[0]],
                         key=lambda x: x[0])
        if len(detected) < 6:
            return None

        result = {}
        for i in range(6):
            x, y, r = detected[i]
            result[i + 1] = (x, y)
            cv2.circle(img, (x, y), r, self._RING_COLOR, self._RING_THICKNESS)
            cv2.putText(img, str(i + 1), (x - 5, y + 5),
                        self._LABEL_FONT, self._LABEL_SCALE, self._RING_COLOR, self._RING_THICKNESS)
        return result
