"""
融合检测器模块
两阶段目标检测与精确定位：
1. YOLOv8 初步检测 → 边界框
2. 传统CV 精确定位 → 亚像素中心点
统一调度两个阶段，输出融合结果
"""
import logging
import time
import numpy as np

from .yolo_detector import YOLOv8Detector, YOLO_AVAILABLE
from .region_extractor import RegionExtractor
from .precise_locator import PreciseLocator

logger = logging.getLogger(__name__)


class FusionResult:
    """融合检测结果数据类"""

    __slots__ = ('yolo_bbox', 'yolo_conf', 'class_id', 'class_name',
                 'precise_center', 'precise_bbox', 'precise_area',
                 'locate_method', 'yolo_center', 'offset_px')

    def __init__(self, yolo_bbox, yolo_conf, class_id, class_name,
                 yolo_center, precise_center=None, precise_bbox=None,
                 precise_area=None, locate_method=None, offset_px=None):
        self.yolo_bbox = yolo_bbox
        self.yolo_conf = yolo_conf
        self.class_id = class_id
        self.class_name = class_name
        self.yolo_center = yolo_center
        self.precise_center = precise_center
        self.precise_bbox = precise_bbox
        self.precise_area = precise_area
        self.locate_method = locate_method
        # 精确定位相对YOLO中心的偏移 (像素)
        self.offset_px = offset_px

    @property
    def has_precise(self):
        return self.precise_center is not None

    @property
    def final_center(self):
        """最终中心点：优先使用精确定位，否则用YOLO中心"""
        return self.precise_center if self.has_precise else self.yolo_center

    def __repr__(self):
        return (f"FusionResult(cls={self.class_name}, yolo={self.yolo_bbox}, "
                f"precise={self.precise_center}, offset={self.offset_px})")


class FusionDetector:
    """两阶段融合检测器：YOLOv8 + 传统CV"""

    def __init__(self, yolo_model='yolov8n.pt', conf_threshold=0.5,
                 locate_method='contour', padding_ratio=0.1, device=None,
                 enable_precise=True):
        """
        :param yolo_model: YOLOv8模型路径
        :param conf_threshold: YOLO置信度阈值
        :param locate_method: 精确定位方法 'contour'/'edge'/'template'/'corner'
        :param padding_ratio: ROI扩展比例
        :param device: 推理设备
        :param enable_precise: 是否启用第二阶段精确定位
        """
        if not YOLO_AVAILABLE:
            raise RuntimeError(
                "ultralytics未安装。请执行: pip install ultralytics"
            )

        self.yolo = YOLOv8Detector(
            model_path=yolo_model, conf_threshold=conf_threshold, device=device
        )
        self.extractor = RegionExtractor(padding_ratio=padding_ratio)
        self.locator = PreciseLocator(method=locate_method)
        self.enable_precise = enable_precise
        logger.info(f"融合检测器初始化完成 (定位方法={locate_method})")

    def set_template(self, template):
        """设置模板（method='template'时使用）"""
        self.locator.set_template(template)

    def detect(self, img, classes=None):
        """
        两阶段检测

        :param img: BGR图像
        :param classes: 只检测指定类别
        :return: List[FusionResult]
        """
        if img is None:
            return []

        # 阶段1：YOLOv8初步检测
        detections = self.yolo.detect(img, classes=classes)
        if not detections:
            return []

        results = []
        for det in detections:
            result = FusionResult(
                yolo_bbox=det.bbox, yolo_conf=det.confidence,
                class_id=det.class_id, class_name=det.class_name,
                yolo_center=det.center
            )

            # 阶段2：传统CV精确定位
            if self.enable_precise:
                self._refine(img, det, result)

            results.append(result)

        return results

    def _refine(self, img, detection, result):
        """对单个YOLO检测结果做精确定位"""
        roi, offset = self.extractor.extract(img, detection.bbox)
        if roi is None:
            return

        precise = self.locator.locate(roi)
        if precise is None:
            return

        # ROI局部坐标 → 原图全局坐标
        global_center = self.extractor.to_global(precise.center, offset)
        global_bbox = self.extractor.to_global_bbox(precise.bbox, offset)

        result.precise_center = global_center
        result.precise_bbox = global_bbox
        result.precise_area = precise.area
        result.locate_method = precise.method

        # 计算偏移量
        ycx, ycy = result.yolo_center
        pcx, pcy = global_center
        result.offset_px = (pcx - ycx, pcy - ycy)

    def warmup(self):
        """模型预热"""
        self.yolo.warmup()


class FusionDetectorWithTiming:
    """带耗时统计的融合检测器装饰器"""

    def __init__(self, detector):
        self.detector = detector
        self.last_yolo_ms = 0.0
        self.last_precise_ms = 0.0
        self.last_total_ms = 0.0

    def detect(self, img, classes=None):
        t0 = time.time()
        results = self.detector.detect(img, classes=classes)
        self.last_total_ms = (time.time() - t0) * 1000
        return results

    def __getattr__(self, name):
        return getattr(self.detector, name)
