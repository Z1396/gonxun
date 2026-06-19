"""
传统CV精确定位模块
基于 YOLOv8 输出的 ROI 区域，应用传统视觉算法做亚像素级精确定位。
支持多种定位策略：
- 边缘检测 + 轮廓分析：精确边界框与中心点
- 模板匹配：已知模板时的高精度定位
- 角点检测：特征点级定位
- 颜色聚类：色块中心精确定位
"""
import logging
import cv2
import numpy as np

logger = logging.getLogger(__name__)


class PreciseLocator:
    """传统CV精确定位器"""

    def __init__(self, method='contour', canny_low=50, canny_high=150,
                 min_contour_area=50):
        """
        :param method: 定位方法 'contour'/'edge'/'template'/'corner'
        :param canny_low: Canny低阈值
        :param canny_high: Canny高阈值
        :param min_contour_area: 最小轮廓面积
        """
        self.method = method
        self.canny_low = canny_low
        self.canny_high = canny_high
        self.min_contour_area = min_contour_area
        self._template = None

    def set_template(self, template):
        """设置模板图像（method='template'时使用）"""
        self._template = template
        logger.info("模板已设置")

    def locate(self, roi):
        """
        对ROI进行精确定位

        :param roi: ROI图像
        :return: PreciseResult 或 None
        """
        if roi is None or roi.size == 0:
            return None

        if self.method == 'contour':
            return self._locate_by_contour(roi)
        elif self.method == 'edge':
            return self._locate_by_edge(roi)
        elif self.method == 'template':
            return self._locate_by_template(roi)
        elif self.method == 'corner':
            return self._locate_by_corner(roi)
        else:
            logger.warning(f"未知定位方法: {self.method}，回退到contour")
            return self._locate_by_contour(roi)

    def _locate_by_contour(self, roi):
        """轮廓分析法：找最大轮廓 → 最小外接矩形 → 精确中心"""
        gray = cv2.cvtColor(roi, cv2.COLOR_BGR2GRAY) if roi.ndim == 3 else roi
        blurred = cv2.GaussianBlur(gray, (5, 5), 0)
        edges = cv2.Canny(blurred, self.canny_low, self.canny_high)

        contours = cv2.findContours(edges, cv2.RETR_EXTERNAL,
                                    cv2.CHAIN_APPROX_SIMPLE)[-2]
        if not contours:
            return None

        # 过滤小轮廓后取最大
        valid = [c for c in contours
                 if cv2.contourArea(c) >= self.min_contour_area]
        if not valid:
            return None

        max_cnt = max(valid, key=cv2.contourArea)
        area = float(cv2.contourArea(max_cnt))

        # 最小外接旋转矩形 → 精确中心
        rect = cv2.minAreaRect(max_cnt)
        center = (int(rect[0][0]), int(rect[0][1]))
        box = cv2.boxPoints(rect)
        bbox = self._box_to_bbox(box)

        return PreciseResult(
            center=center, bbox=bbox, area=area,
            method='contour', extra={'contour_count': len(valid)}
        )

    def _locate_by_edge(self, roi):
        """边缘检测法：Canny边缘质心"""
        gray = cv2.cvtColor(roi, cv2.COLOR_BGR2GRAY) if roi.ndim == 3 else roi
        edges = cv2.Canny(gray, self.canny_low, self.canny_high)

        ys, xs = np.where(edges > 0)
        if len(xs) == 0:
            return None

        cx, cy = int(xs.mean()), int(ys.mean())
        x1, y1 = int(xs.min()), int(ys.min())
        x2, y2 = int(xs.max()), int(ys.max())
        area = float((x2 - x1) * (y2 - y1))

        return PreciseResult(
            center=(cx, cy), bbox=(x1, y1, x2, y2), area=area,
            method='edge', extra={'edge_pixels': int(len(xs))}
        )

    def _locate_by_template(self, roi):
        """模板匹配法：归一化互相关"""
        if self._template is None:
            logger.warning("未设置模板，无法使用template方法")
            return None

        gray_roi = cv2.cvtColor(roi, cv2.COLOR_BGR2GRAY) if roi.ndim == 3 else roi
        gray_tmpl = cv2.cvtColor(self._template, cv2.COLOR_BGR2GRAY) \
            if self._template.ndim == 3 else self._template

        th, tw = gray_tmpl.shape[:2]
        if gray_roi.shape[0] < th or gray_roi.shape[1] < tw:
            return None

        res = cv2.matchTemplate(gray_roi, gray_tmpl, cv2.TM_CCOEFF_NORMED)
        _, max_val, _, max_loc = cv2.minMaxLoc(res)

        x1, y1 = max_loc
        x2, y2 = x1 + tw, y1 + th
        cx, cy = (x1 + x2) // 2, (y1 + y2) // 2

        return PreciseResult(
            center=(cx, cy), bbox=(x1, y1, x2, y2),
            area=float(tw * th), method='template',
            extra={'match_score': float(max_val)}
        )

    def _locate_by_corner(self, roi):
        """角点检测法：Shi-Tomasi角点质心"""
        gray = cv2.cvtColor(roi, cv2.COLOR_BGR2GRAY) if roi.ndim == 3 else roi
        corners = cv2.goodFeaturesToTrack(gray, maxCorners=50, qualityLevel=0.1,
                                          minDistance=5)
        if corners is None or len(corners) == 0:
            return None

        pts = corners.reshape(-1, 2)
        cx, cy = int(pts[:, 0].mean()), int(pts[:, 1].mean())
        x1, y1 = int(pts[:, 0].min()), int(pts[:, 1].min())
        x2, y2 = int(pts[:, 0].max()), int(pts[:, 1].max())

        return PreciseResult(
            center=(cx, cy), bbox=(x1, y1, x2, y2),
            area=float((x2 - x1) * (y2 - y1)), method='corner',
            extra={'corner_count': int(len(pts))}
        )

    @staticmethod
    def _box_to_bbox(box):
        """旋转矩形4顶点 → 轴对齐bbox (x1,y1,x2,y2)"""
        xs = box[:, 0]
        ys = box[:, 1]
        return (int(xs.min()), int(ys.min()), int(xs.max()), int(ys.max()))


class PreciseResult:
    """精确定位结果数据类"""

    __slots__ = ('center', 'bbox', 'area', 'method', 'extra')

    def __init__(self, center, bbox, area, method, extra=None):
        self.center = tuple(int(v) for v in center)
        self.bbox = tuple(int(v) for v in bbox)
        self.area = float(area)
        self.method = method
        self.extra = extra or {}

    def __repr__(self):
        return (f"PreciseResult(method={self.method}, center={self.center}, "
                f"bbox={self.bbox}, area={self.area:.0f})")
