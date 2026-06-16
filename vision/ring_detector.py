"""
圆环检测模块
- 3色环定位 (色环标定用)
- 6环识别 (粗加工区/暂存区，按X排序映射1~6)
- 6环评分表 (比赛规则表3)
"""
import cv2
import numpy as np


# 6环尺寸评分表 (比赛规则表3)
# 1环15分、2环10分、3环7分、4环5分、5环3分、6环1分
# 6环外及物料倾倒得0分
RING_SCORES = {1: 15, 2: 10, 3: 7, 4: 5, 5: 3, 6: 1}


def calc_placement_score(ring_id, material_fallen=False):
    """
    计算放置得分
    :param ring_id: 环号(1~6)，0或None表示6环外
    :param material_fallen: 物料是否倾倒
    :return: 得分
    """
    if material_fallen:
        return 0
    if ring_id is None or ring_id < 1 or ring_id > 6:
        return 0
    return RING_SCORES.get(ring_id, 0)


class ThreeRingDetector:
    """
    三色定位环检测器
    用于底盘定位：识别3个标记色环，按X排序
    """
    def detect(self, img):
        """
        霍夫圆检测，识别三个定位色环标记
        :param img: 输入BGR图像
        :return: (x1,y1,x2,y2,x3,y3) 按X从左到右排序；检测不到3个圆返回None
        """
        if img is None:
            return None

        erode_hsv = cv2.erode(img, None, iterations=2)
        kernel = np.ones((7, 7), np.uint8)
        diRange_hsv = cv2.dilate(erode_hsv, kernel, 1)
        gray_img = cv2.cvtColor(diRange_hsv, cv2.COLOR_BGR2GRAY)

        # CLAHE直方图均衡
        clahe = cv2.createCLAHE(clipLimit=5.0, tileGridSize=(8, 8))
        clahed = clahe.apply(gray_img)

        # 形态学梯度
        kernel_grad = cv2.getStructuringElement(cv2.MORPH_ELLIPSE, (5, 5))
        gradient = cv2.morphologyEx(gray_img, cv2.MORPH_GRADIENT, kernel_grad)

        # 高斯+增强+阈值分割
        result = cv2.GaussianBlur(gradient, (7, 7), 3, 3)
        eqal_img = cv2.convertScaleAbs(result, alpha=4, beta=0)
        eqal_img = cv2.GaussianBlur(eqal_img, (7, 7), 3, 3)

        retval, threshold_img = cv2.threshold(eqal_img, 70, 255, cv2.THRESH_BINARY)
        threshold_img = cv2.GaussianBlur(threshold_img, (9, 9), 3, 3)

        circles = cv2.HoughCircles(
            threshold_img,
            cv2.HOUGH_GRADIENT_ALT,
            dp=1.5,
            minDist=50,
            param1=100,
            param2=0.95,
            minRadius=15,
            maxRadius=50
        )

        if circles is not None and len(circles[0]) == 3:
            circles = np.uint16(np.around(circles))

            for circle in circles[0, :]:
                cx, cy, r = circle
                cv2.circle(img, (cx, cy), r, (0, 0, 255), 2)
                cv2.circle(img, (cx, cy), 2, (255, 0, 0), 2)

            # 正确提取每个圆的(x,y)坐标，按x排序
            circle_all = [[c[0], c[1]] for c in circles[0]]
            circle_list = sorted(circle_all, key=lambda x: x[0])

            return (
                circle_list[0][0], circle_list[0][1],
                circle_list[1][0], circle_list[1][1],
                circle_list[2][0], circle_list[2][1]
            )
        return None


class SixRingDetector:
    """
    6环检测器：识别粗加工区/暂存区的6个圆环位置
    按X坐标从左到右映射到1~6号环
    """
    def detect(self, img):
        """
        检测6个圆环位置
        :param img: 输入BGR图像(粗加工区或暂存区)
        :return: 圆环位置字典 {ring_id: (x,y), ...} 检测失败返回None
        """
        if img is None:
            return None
        gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)
        blurred = cv2.GaussianBlur(gray, (5, 5), 0)
        edges = cv2.Canny(blurred, 50, 150)

        circles = cv2.HoughCircles(
            edges, cv2.HOUGH_GRADIENT, dp=1, minDist=40,
            param1=100, param2=30, minRadius=20, maxRadius=120
        )
        if circles is None:
            return None

        # 按x坐标排序，从左到右对应1~6号环
        detected = sorted([(int(c[0]), int(c[1]), int(c[2])) for c in circles[0]],
                          key=lambda x: x[0])
        if len(detected) < 6:
            return None

        result = {}
        for i in range(6):
            x, y, r = detected[i]
            result[i + 1] = (x, y)
            cv2.circle(img, (x, y), r, (255, 0, 255), 2)
            cv2.putText(img, str(i + 1), (x - 5, y + 5),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 0, 255), 2)
        return result
