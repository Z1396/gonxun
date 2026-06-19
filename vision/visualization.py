"""
可视化输出模块
绘制 YOLOv8 边界框 + 精确定位中心点 + 偏移向量
支持单帧图像标注和实时视频流显示
"""
import cv2
import numpy as np

from .yolo_detector import Detection
from .fusion_detector import FusionResult


# 颜色方案 (BGR)
COLOR_YOLO = (0, 165, 255)      # 橙色 - YOLO边界框
COLOR_PRECISE = (0, 255, 0)     # 绿色 - 精确定位
COLOR_CENTER_YOLO = (0, 0, 255) # 红色 - YOLO中心
COLOR_OFFSET = (255, 0, 255)    # 紫色 - 偏移向量
COLOR_TEXT_BG = (0, 0, 0)       # 黑色 - 文字背景


class Visualizer:
    """检测结果可视化器"""

    def __init__(self, show_offset=True, show_conf=True, font_scale=0.5):
        """
        :param show_offset: 是否绘制YOLO中心→精确中心的偏移向量
        :param show_conf: 是否显示置信度
        :param font_scale: 字体大小
        """
        self.show_offset = show_offset
        self.show_conf = show_conf
        self.font_scale = font_scale

    def draw_detections(self, img, detections):
        """绘制YOLOv8检测结果"""
        if img is None or not detections:
            return img
        out = img.copy()
        for det in detections:
            self._draw_yolo_box(out, det)
        return out

    def draw_fusion_results(self, img, results):
        """绘制融合检测结果（YOLO框 + 精确中心 + 偏移向量）"""
        if img is None or not results:
            return img
        out = img.copy()
        for res in results:
            self._draw_yolo_box(out, res)
            if res.has_precise:
                self._draw_precise(out, res)
        return out

    def _draw_yolo_box(self, img, item):
        """绘制YOLO边界框"""
        if hasattr(item, 'yolo_bbox'):
            bbox = item.yolo_bbox
            conf = item.yolo_conf
            name = item.class_name
            center = item.yolo_center
        else:  # Detection对象
            bbox = item.bbox
            conf = item.confidence
            name = item.class_name
            center = item.center

        x1, y1, x2, y2 = bbox
        cv2.rectangle(img, (x1, y1), (x2, y2), COLOR_YOLO, 2)

        # YOLO中心点
        cv2.circle(img, center, 4, COLOR_CENTER_YOLO, -1)

        # 标签
        label = name
        if self.show_conf:
            label = f"{name} {conf:.2f}"
        self._draw_label(img, label, (x1, max(0, y1 - 8)))

    def _draw_precise(self, img, result):
        """绘制精确定位结果"""
        # 精确中心点 (大十字)
        cx, cy = result.precise_center
        cv2.drawMarker(img, (cx, cy), COLOR_PRECISE,
                       markerType=cv2.MARKER_CROSS, markerSize=20, thickness=2)

        # 精确边界框
        if result.precise_bbox:
            x1, y1, x2, y2 = result.precise_bbox
            cv2.rectangle(img, (x1, y1), (x2, y2), COLOR_PRECISE, 1)

        # 偏移向量
        if self.show_offset and result.offset_px is not None:
            cv2.arrowedLine(img, result.yolo_center, (cx, cy),
                            COLOR_OFFSET, 1, tipLength=0.2)
            ox, oy = result.offset_px
            self._draw_label(img, f"d=({ox},{oy})", (cx + 8, cy + 8))

        # 方法标签
        self._draw_label(img, f"[{result.locate_method}]",
                         (cx + 8, cy + 22), color=COLOR_PRECISE)

    def _draw_label(self, img, text, org, color=COLOR_YOLO):
        """绘制带背景的文字"""
        font = cv2.FONT_HERSHEY_SIMPLEX
        scale = self.font_scale
        thickness = 1
        (tw, th), baseline = cv2.getTextSize(text, font, scale, thickness)

        x, y = org
        cv2.rectangle(img, (x, y - th - 2), (x + tw + 2, y + 2),
                      COLOR_TEXT_BG, -1)
        cv2.putText(img, text, (x + 1, y), font, scale, color, thickness,
                    cv2.LINE_AA)

    @staticmethod
    def draw_fps(img, fps):
        """在右上角绘制FPS"""
        cv2.putText(img, f"FPS: {fps:.1f}", (img.shape[1] - 100, 25),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)
        return img

    @staticmethod
    def draw_stage_info(img, yolo_ms, precise_ms, total_ms):
        """在左上角绘制阶段耗时"""
        info = [
            f"YOLO: {yolo_ms:.1f}ms",
            f"Refine: {precise_ms:.1f}ms",
            f"Total: {total_ms:.1f}ms",
        ]
        for i, line in enumerate(info):
            cv2.putText(img, line, (10, 20 + i * 20),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 0), 1)
        return img
