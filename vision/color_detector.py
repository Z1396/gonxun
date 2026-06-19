"""
颜色识别模块
6种物料颜色HSV阈值定义与色块中心检测
比赛规则：红1/黄2/蓝3/绿4/黑5/浅蓝6
"""
import cv2
import numpy as np


# 所有颜色HSV阈值库，红色占两段色相区间
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

# 比赛编号映射：数字ID → 颜色key、中文名称
COLOR_ID_MAP = {
    1: ('red', '红色'),
    2: ('yellow', '黄色'),
    3: ('blue', '蓝色'),
    4: ('green', '绿色'),
    5: ('black', '黑色'),
    6: ('light_blue', '浅蓝')
}


def get_color_by_id(color_id):
    """根据颜色编号(1~6)获取英文key和中文名"""
    return COLOR_ID_MAP.get(color_id, (None, None))


class ColorDetector:
    """颜色检测器：识别6种比赛物料颜色，返回色块中心坐标"""

    # 类内私有算法常量，约定运行时不修改
    _BLUR_KSIZE = (5, 5)               # 高斯模糊卷积核尺寸
    _ERODE_KERNEL = np.ones((3, 3), np.uint8)  # 腐蚀操作核
    _ERODE_ITER = 2                    # 腐蚀迭代次数

    def __init__(self):
        # 绑定全局颜色阈值配置
        self.color_dist = COLOR_DIST

    def _make_mask(self, hsv, color):
        """生成HSV二值掩码，红色合并两段色相区间"""
        dist = self.color_dist[color]
        if color == 'red':
            '''cv2.inRange(src, lowerb, upperb[, dst]) -> dst
            参数说明
            src：输入图像（单通道 / 多通道均可，你的项目用HSV 三通道图）
            lowerb：各通道下限阈值，np.array([H,S,V])
            upperb：各通道上限阈值，np.array([H,S,V])
            返回值 dst：二值掩码图像（单通道）
            像素在 [lower, upper] 区间内 → 白色 255
            像素不在区间内 → 黑色 0'''
            mask1 = cv2.inRange(hsv, dist['Lower1'], dist['Upper1'])
            mask2 = cv2.inRange(hsv, dist['Lower2'], dist['Upper2'])
            return mask1 | mask2
        return cv2.inRange(hsv, dist['Lower'], dist['Upper'])

    def _preprocess(self, img):
        """图像预处理流水线：模糊降噪 → BGR转HSV → 腐蚀消除细小噪点"""
        # 高斯模糊平滑画面
        blur_img = cv2.GaussianBlur(img, self._BLUR_KSIZE, 0)
        # 转换至HSV色彩空间用于颜色分割
        hsv_img = cv2.cvtColor(blur_img, cv2.COLOR_BGR2HSV)
        # 腐蚀操作过滤微小杂点
        erode_img = cv2.erode(hsv_img, self._ERODE_KERNEL, iterations=self._ERODE_ITER)
        return erode_img

    def detect(self, img, color, min_area=2000, max_area=10000):
        """
        对外主接口：检测单种颜色，返回色块中心像素坐标
        :param img: 原始BGR图像帧
        :param color: 颜色字符串key，如'red'/'blue'
        :param min_area: 色块最小有效像素面积，小于则判定为噪声
        :param max_area: 色块最大有效像素面积，大于则判定为异常
        :return: (x, y) 中心点整数坐标；无目标返回None
        """
        # 非法输入直接拦截
        if img is None or color not in self.color_dist:
            return None

        # 预处理图像
        hsv = self._preprocess(img)
        # 生成对应颜色掩码
        mask = self._make_mask(hsv, color)
        # 查找所有外层轮廓，兼容所有OpenCV版本
        # cv2.findContours：在二值掩码图像中查找白色连通区域的轮廓线
        # 参数1 mask.copy()：拷贝一份掩码图像传入函数
        # 原因：findContours会直接修改输入图像，拷贝可以保留原始mask，后续还能复用原图掩码
        # 参数2 cv2.RETR_EXTERNAL：轮廓检索模式，只保留物体最外层轮廓，自动忽略色块内部孔洞、内层小轮廓，减少干扰
        # 参数3 cv2.CHAIN_APPROX_SIMPLE：轮廓点压缩算法，直线只存首尾点、矩形只存四个角，丢弃中间冗余像素，减少计算量
        # findContours返回元组 (轮廓列表, 层级信息)（新版OpenCV）/ (图像, 轮廓列表, 层级信息)（旧OpenCV2）
        # [-2] 取元组倒数第二个元素，无论返回2个还是3个返回值，都稳定拿到轮廓列表，实现全版本兼容
        contours = cv2.findContours(mask.copy(), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)[-2]

        # 画面无对应色块
        if len(contours) == 0:
            return None

        # 筛选面积最大的色块作为目标
        max_contour = max(contours, key=cv2.contourArea)
        area = int(cv2.contourArea(max_contour))
        # 最大面积异常检查
        if area > max_area:
            return None
        

        # 过滤小面积噪声
        if area < min_area:
            return None

        # 最小外接矩形获取色块中心
        rect = cv2.minAreaRect(max_contour)
        x, y = rect[0]
        return int(x), int(y)


if __name__ == "__main__":
    detector = ColorDetector()
    cap = cv2.VideoCapture(1)

    while True:
        ret, frame = cap.read()
        if not ret:
            break

        # 预处理 + 生成掩码
        hsv = detector._preprocess(frame)
        mask = detector._make_mask(hsv, "blue")

        # 查找轮廓
        contours = cv2.findContours(mask.copy(), cv2.RETR_EXTERNAL, cv2.CHAIN_APPROX_SIMPLE)[-2]

        # 绘制所有轮廓 + 最大轮廓矩形框 + 每个轮廓面积
        contour_frame = frame.copy()
        if contours:
            max_contour = max(contours, key=cv2.contourArea)
            for cnt in contours:
                area = int(cv2.contourArea(cnt))
                x, y, w, h = cv2.boundingRect(cnt)
                cv2.rectangle(contour_frame, (x, y), (x + w, y + h), (0, 255, 0), 2)
                cv2.putText(contour_frame, str(area), (x, y - 5),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 255), 1)
            # 最大轮廓用红色框高亮
            x, y, w, h = cv2.boundingRect(max_contour)
            cv2.rectangle(contour_frame, (x, y), (x + w, y + h), (0, 0, 255), 3)

        # 检测色块中心
        pos = detector.detect(frame, "blue", min_area=2000)
        if pos:
            cx, cy = pos
            cv2.circle(frame, (cx, cy), 8, (0, 0, 255), -1)
            cv2.putText(frame, f"BLUE ({cx},{cy})", (cx - 8, cy - 12),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 0, 255), 1)

        cv2.imshow("Color Detect Test", frame)
        cv2.imshow("Mask", mask)
        cv2.imshow("Contours", contour_frame)

        if cv2.waitKey(1) & 0xFF == ord('q'):
            break

    cap.release()
    cv2.destroyAllWindows()
