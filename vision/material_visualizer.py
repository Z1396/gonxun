"""
物料识别可视化模块
绘制 YOLO 边界框 + 颜色标签 + 精确中心点 + 颜色掩码
"""
import cv2
import numpy as np

from .material_recognizer import MaterialResult


# 颜色标注方案 (BGR)
COLOR_MAP = {
    'red': (0, 0, 255),
    'yellow': (0, 255, 255),
    'blue': (255, 0, 0),
    'green': (0, 255, 0),
    'black': (128, 128, 128),
    'light_blue': (255, 165, 0),
}
COLOR_BOX = (0, 165, 255)       # 橙色 - YOLO边界框
COLOR_CENTER = (0, 255, 255)    # 黄色 - 精确中心
COLOR_TEXT_BG = (0, 0, 0)       # 黑色 - 文字背景


class MaterialVisualizer:
    """物料识别可视化器"""

    def __init__(self, show_conf=True, show_color_conf=True, font_scale=0.5):
        """
        :param show_conf: 是否显示YOLO置信度
        :param show_color_conf: 是否显示颜色置信度
        :param font_scale: 字体大小
        """
        self.show_conf = show_conf
        self.show_color_conf = show_color_conf
        self.font_scale = font_scale

    def draw_results(self, img, results):
        """绘制物料识别结果"""
        if img is None or not results:
            return img
        out = img.copy()
        for res in results:
            self._draw_box(out, res)
            if res.has_color:
                self._draw_color_info(out, res)
        return out

    def draw_color_masks(self, img, results):
        """绘制所有检测到的颜色掩码（拼接显示）"""
        masks = []
        for res in results:
            if res.has_color and res.mask is not None:
                mask_colored = cv2.cvtColor(res.mask, cv2.COLOR_GRAY2BGR)
                color_bgr = COLOR_MAP.get(res.color_name, (255, 255, 255))
                mask_colored = cv2.bitwise_and(mask_colored, mask_colored,
                                               mask=res.mask)
                mask_colored[res.mask > 0] = color_bgr
                label = f"{res.color_name} ({res.color_conf:.1%})"
                cv2.putText(mask_colored, label, (5, 20),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.5, (255, 255, 255), 1)
                masks.append(mask_colored)

        if not masks:
            return img

        # 垂直拼接所有掩码
        max_w = max(m.shape[1] for m in masks)
        resized = []
        for m in masks:
            if m.shape[1] < max_w:
                pad = np.zeros((m.shape[0], max_w - m.shape[1], 3), dtype=np.uint8)
                m = np.hstack([m, pad])
            resized.append(m)

        return np.vstack(resized)

    def _draw_box(self, img, result):
        """绘制YOLO边界框"""
        x1, y1, x2, y2 = result.yolo_bbox
        cv2.rectangle(img, (x1, y1), (x2, y2), COLOR_BOX, 2)

        # 标签
        label = result.yolo_class
        if self.show_conf:
            label = f"{label} {result.yolo_conf:.2f}"
        self._draw_label(img, label, (x1, max(0, y1 - 8)), color=COLOR_BOX)

    def _draw_color_info(self, img, result):
        """绘制颜色信息"""
        # 精确中心点
        if result.center:
            cx, cy = result.center
            cv2.drawMarker(img, (cx, cy), COLOR_CENTER,
                           markerType=cv2.MARKER_CROSS, markerSize=15, thickness=2)

        # 颜色标签
        color_bgr = COLOR_MAP.get(result.color_name, (255, 255, 255))
        label = result.color_name
        if self.show_color_conf:
            label = f"{label} {result.color_conf:.1%}"
        if result.color_id:
            label = f"[{result.color_id}] {label}"

        x1, _, _, y2 = result.yolo_bbox
        self._draw_label(img, label, (x1, y2 + 15), color=color_bgr)

    def _draw_label(self, img, text, org, color=COLOR_BOX):
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
    def draw_summary(img, results):
        """在左侧绘制识别摘要"""
        if not results:
            cv2.putText(img, "No materials detected", (10, 20),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 0, 255), 1)
            return img

        lines = [f"Detected: {len(results)}"]
        for i, r in enumerate(results):
            lines.append(f"  {i+1}. {r.label}")
            if r.center:
                lines.append(f"     center=({r.center[0]},{r.center[1]})")

        for i, line in enumerate(lines):
            cv2.putText(img, line, (10, 20 + i * 18),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.4, (255, 255, 0), 1)
        return img
