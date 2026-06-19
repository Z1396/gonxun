"""
YOLOv8 目标初步检测模块
- 加载预训练或自训练 YOLOv8 模型
- 对输入图像/视频流进行目标检测
- 输出边界框坐标、类别、置信度

依赖：ultralytics >= 8.0.0, torch
未安装时给出明确提示，支持优雅降级
"""
import logging
import numpy as np

logger = logging.getLogger(__name__)

try:
    from ultralytics import YOLO
    YOLO_AVAILABLE = True
except ImportError:
    YOLO_AVAILABLE = False
    logger.warning("ultralytics未安装，YOLOv8检测不可用。安装: pip install ultralytics")


class Detection:
    """单个检测结果数据类"""

    __slots__ = ('bbox', 'confidence', 'class_id', 'class_name')

    def __init__(self, bbox, confidence, class_id, class_name):
        """
        :param bbox: 边界框 (x1, y1, x2, y2) 像素坐标
        :param confidence: 置信度 0~1
        :param class_id: 类别ID
        :param class_name: 类别名
        """
        self.bbox = tuple(int(v) for v in bbox)
        self.confidence = float(confidence)
        self.class_id = int(class_id)
        self.class_name = class_name

    @property
    def center(self):
        """返回中心点 (cx, cy)"""
        x1, y1, x2, y2 = self.bbox
        return ((x1 + x2) // 2, (y1 + y2) // 2)

    @property
    def size(self):
        """返回宽高 (w, h)"""
        x1, y1, x2, y2 = self.bbox
        return (x2 - x1, y2 - y1)

    def __repr__(self):
        return (f"Detection(cls={self.class_name}, conf={self.confidence:.2f}, "
                f"bbox={self.bbox})")


class YOLOv8Detector:
    """YOLOv8 目标检测器"""

    def __init__(self, model_path='yolov8n.pt', conf_threshold=0.5, iou_threshold=0.45,
                 device=None, imgsz=640):
        """
        :param model_path: 模型权重路径 (.pt) 或模型规格名 (yolov8n/s/m/l/x)
        :param conf_threshold: 置信度阈值
        :param iou_threshold: NMS IoU 阈值
        :param device: 推理设备 'cpu'/'cuda:0'/None(自动)
        :param imgsz: 推理图像尺寸
        """
        if not YOLO_AVAILABLE:
            raise RuntimeError(
                "ultralytics未安装，无法使用YOLOv8。请执行: pip install ultralytics"
            )
        self.model_path = model_path
        self.conf_threshold = conf_threshold
        self.iou_threshold = iou_threshold
        self.device = device
        self.imgsz = imgsz
        self.model = YOLO(model_path)
        logger.info(f"YOLOv8模型已加载: {model_path}")

    def detect(self, img, classes=None, verbose=False):
        """
        对单帧图像进行目标检测

        :param img: BGR图像 (H,W,3) 或路径
        :param classes: 只检测指定类别ID列表；None=全部
        :param verbose: 是否打印推理日志
        :return: List[Detection]
        """
        if img is None:
            return []

        results = self.model.predict(
            source=img, conf=self.conf_threshold, iou=self.iou_threshold,
            imgsz=self.imgsz, device=self.device, classes=classes,
            verbose=verbose
        )
        return self._parse_results(results[0])

    @staticmethod
    def _parse_results(result):
        """解析ultralytics结果对象为Detection列表"""
        detections = []
        if result.boxes is None or len(result.boxes) == 0:
            return detections

        boxes = result.boxes.xyxy.cpu().numpy()
        confs = result.boxes.conf.cpu().numpy()
        cls_ids = result.boxes.cls.cpu().numpy().astype(int)
        names = result.names

        for bbox, conf, cls_id in zip(boxes, confs, cls_ids):
            detections.append(Detection(
                bbox=bbox, confidence=conf,
                class_id=cls_id, class_name=names.get(cls_id, str(cls_id))
            ))
        return detections

    def warmup(self):
        """模型预热，避免首帧延迟"""
        dummy = np.zeros((self.imgsz, self.imgsz, 3), dtype=np.uint8)
        self.detect(dummy, verbose=False)
        logger.info("YOLOv8模型预热完成")
