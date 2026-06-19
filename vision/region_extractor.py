"""
区域提取模块
基于 YOLOv8 输出的边界框，从原图裁剪目标区域 (ROI)，
供传统CV算法做精确定位。支持：
- 边界框裁剪
- 边界扩展 (padding)，避免目标贴边导致轮廓缺失
- 越界保护
- 坐标系映射 (ROI局部坐标 → 原图全局坐标)
"""
import logging
import numpy as np

logger = logging.getLogger(__name__)


class RegionExtractor:
    """ROI区域提取器"""

    def __init__(self, padding_ratio=0.1, min_size=32):
        """
        :param padding_ratio: 边界框向外扩展比例 (0.1=10%)
        :param min_size: ROI最小边长，小于此值则跳过
        """
        self.padding_ratio = padding_ratio
        self.min_size = min_size

    def extract(self, img, bbox):
        """
        从原图提取ROI

        :param img: 原始BGR图像
        :param bbox: (x1, y1, x2, y2)
        :return: (roi, global_offset) 或 (None, None)
                 global_offset = (offset_x, offset_y) ROI左上角在原图的坐标
        """
        if img is None:
            return None, None

        h, w = img.shape[:2]
        x1, y1, x2, y2 = bbox

        # 扩展边界
        bw = x2 - x1
        bh = y2 - y1
        pad_w = int(bw * self.padding_ratio)
        pad_h = int(bh * self.padding_ratio)

        x1 = max(0, x1 - pad_w)
        y1 = max(0, y1 - pad_h)
        x2 = min(w, x2 + pad_w)
        y2 = min(h, y2 + pad_h)

        # 尺寸检查
        if x2 - x1 < self.min_size or y2 - y1 < self.min_size:
            logger.debug(f"ROI尺寸过小: ({x2-x1}, {y2-y1})，跳过")
            return None, None

        roi = img[y1:y2, x1:x2].copy()
        return roi, (x1, y1)

    @staticmethod
    def to_global(local_point, offset):
        """ROI局部坐标 → 原图全局坐标"""
        if local_point is None or offset is None:
            return None
        lx, ly = local_point
        ox, oy = offset
        return (lx + ox, ly + oy)

    @staticmethod
    def to_global_bbox(local_bbox, offset):
        """ROI局部边界框 → 原图全局边界框"""
        if local_bbox is None or offset is None:
            return None
        lx1, ly1, lx2, ly2 = local_bbox
        ox, oy = offset
        return (lx1 + ox, ly1 + oy, lx2 + ox, ly2 + oy)
