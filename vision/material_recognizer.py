"""
物料识别系统：YOLOv8 + 颜色检测融合
工作流程：
1. YOLOv8 检测画面中的物料对象
2. 对每个检测到的对象裁剪 ROI
3. 使用 ColorDetector 分析 ROI 的颜色特征
4. 融合输出：对象类别 + 颜色 + 精确位置 + 置信度
"""
import logging
import cv2
import numpy as np

from .yolo_detector import YOLOv8Detector, YOLO_AVAILABLE
from .color_detector import ColorDetector, COLOR_DIST

logger = logging.getLogger(__name__)


class MaterialResult:
    """物料识别结果数据类"""

    __slots__ = ('yolo_bbox', 'yolo_conf', 'yolo_class', 'color_name',
                 'color_id', 'color_conf', 'center', 'area', 'mask')

    def __init__(self, yolo_bbox, yolo_conf, yolo_class, color_name=None,
                 color_id=None, color_conf=0.0, center=None, area=0, mask=None):
        self.yolo_bbox = yolo_bbox
        self.yolo_conf = yolo_conf
        self.yolo_class = yolo_class
        self.color_name = color_name
        self.color_id = color_id
        self.color_conf = color_conf
        self.center = center
        self.area = area
        self.mask = mask

    @property
    def has_color(self):
        return self.color_name is not None

    @property
    def label(self):
        """完整标签：类别 + 颜色"""
        if self.has_color:
            return f"{self.yolo_class}({self.color_name})"
        return self.yolo_class

    def __repr__(self):
        return (f"MaterialResult(cls={self.yolo_class}, color={self.color_name}, "
                f"conf={self.yolo_conf:.2f}, center={self.center})")


class MaterialRecognizer:
    """物料识别器：YOLOv8 + 颜色检测融合"""

    # 颜色分析参数
    _COLOR_MIN_RATIO = 0.15   # ROI中某颜色像素占比超过此值才判定为该颜色
    _COLOR_PAD_RATIO = 0.05   # ROI裁剪时向内收缩比例，避免背景干扰

    def __init__(self, yolo_model='yolov8n.pt', conf_threshold=0.5,
                 device=None, imgsz=640):
        """
        :param yolo_model: YOLOv8模型路径
        :param conf_threshold: YOLO置信度阈值
        :param device: 推理设备
        :param imgsz: 推理图像尺寸
        """
        if not YOLO_AVAILABLE:
            raise RuntimeError(
                "ultralytics未安装。请执行: pip install ultralytics"
            )

        self.yolo = YOLOv8Detector(
            model_path=yolo_model, conf_threshold=conf_threshold,
            device=device, imgsz=imgsz
        )
        self.color_detector = ColorDetector()
        logger.info("物料识别器初始化完成")

    def recognize(self, img, classes=None):
        """
        对单帧图像进行物料识别

        :param img: BGR图像
        :param classes: 只检测指定YOLO类别ID
        :return: List[MaterialResult]
        """
        if img is None:
            return []

        # 阶段1：YOLOv8 检测
        detections = self.yolo.detect(img, classes=classes)
        if not detections:
            return []

        results = []
        for det in detections:
            result = MaterialResult(
                yolo_bbox=det.bbox, yolo_conf=det.confidence,
                yolo_class=det.class_name
            )

            # 阶段2：颜色分析
            self._analyze_color(img, result)

            results.append(result)

        return results

    def _analyze_color(self, img, result):
        """对单个YOLO检测区域进行颜色分析"""
        h, w = img.shape[:2]
        x1, y1, x2, y2 = result.yolo_bbox

        # ROI向内收缩，避免背景干扰
        bw = x2 - x1
        bh = y2 - y1
        pad_x = int(bw * self._COLOR_PAD_RATIO)
        pad_y = int(bh * self._COLOR_PAD_RATIO)

        roi_x1 = max(0, x1 + pad_x)
        roi_y1 = max(0, y1 + pad_y)
        roi_x2 = min(w, x2 - pad_x)
        roi_y2 = min(h, y2 - pad_y)

        if roi_x2 <= roi_x1 or roi_y2 <= roi_y1:
            roi = img[y1:y2, x1:x2]
        else:
            roi = img[roi_y1:roi_y2, roi_x1:roi_x2]

        if roi.size == 0:
            return

        # 对ROI进行颜色分析
        best_color = None
        best_conf = 0.0
        best_mask = None

        hsv = self.color_detector._preprocess(roi)
        roi_area = roi.shape[0] * roi.shape[1]

        for color_name in COLOR_DIST.keys():
            mask = self.color_detector._make_mask(hsv, color_name)
            # 计算颜色像素占比
            color_pixels = np.count_nonzero(mask)
            ratio = color_pixels / roi_area if roi_area > 0 else 0

            if ratio > self._COLOR_MIN_RATIO and ratio > best_conf:
                best_color = color_name
                best_conf = ratio
                best_mask = mask

        if best_color:
            result.color_name = best_color
            result.color_conf = best_conf
            result.mask = best_mask

            # 获取颜色编号
            for cid, (key, _) in enumerate(
                    [(k, v) for k, v in {
                        'red': 1, 'yellow': 2, 'blue': 3,
                        'green': 4, 'black': 5, 'light_blue': 6
                    }.items()], 1):
                if key == best_color:
                    result.color_id = cid
                    break

            # 计算色块中心
            if best_mask is not None:
                contours = cv2.findContours(
                    best_mask.copy(), cv2.RETR_EXTERNAL,
                    cv2.CHAIN_APPROX_SIMPLE
                )[-2]
                if contours:
                    max_cnt = max(contours, key=cv2.contourArea)
                    rect = cv2.minAreaRect(max_cnt)
                    cx, cy = rect[0]
                    # 转换到原图坐标
                    result.center = (
                        int(cx) + roi_x1,
                        int(cy) + roi_y1
                    )
                    result.area = int(cv2.contourArea(max_cnt))

    def warmup(self):
        """模型预热"""
        self.yolo.warmup()
        logger.info("物料识别器预热完成")
