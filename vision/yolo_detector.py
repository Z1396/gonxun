"""
YOLOv8 目标初步检测模块
- 加载预训练或自训练 YOLOv8 模型
- 对输入图像/视频流进行目标检测
- 输出边界框坐标、类别、置信度

依赖：ultralytics >= 8.0.0, torch
未安装时给出明确提示，支持优雅降级

Jetson Nano 优化：
- 自动检测 Jetson 平台并降低推理尺寸（640→320）
- 支持 FP16 半精度推理
"""
import logging
import os
import numpy as np

logger = logging.getLogger(__name__)

try:
    from ultralytics import YOLO
    YOLO_AVAILABLE = True
except ImportError:
    YOLO_AVAILABLE = False
    logger.warning("ultralytics未安装，YOLOv8检测不可用。安装: pip install ultralytics")


def is_jetson():
    """检测是否在 Jetson 平台上运行"""
    try:
        with open('/etc/nv_tegra_release', 'r') as f:
            return True
    except FileNotFoundError:
        return False
    except Exception:
        return False


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
                 device=None, imgsz=640, half=False):
        """
        :param model_path: 模型权重路径 (.pt) 或模型规格名 (yolov8n/s/m/l/x)
        :param conf_threshold: 置信度阈值
        :param iou_threshold: NMS IoU 阈值
        :param device: 推理设备 'cpu'/'cuda:0'/None(自动)
        :param imgsz: 推理图像尺寸
        :param half: 是否使用FP16半精度推理（Jetson推荐True）
        """
        if not YOLO_AVAILABLE:
            raise RuntimeError(
                "ultralytics未安装，无法使用YOLOv8。请执行: pip install ultralytics"
            )

        # Jetson 平台自动优化
        self._is_jetson = is_jetson()
        if self._is_jetson and imgsz > 320:
            logger.info(f"[Jetson] 推理尺寸自动降级: {imgsz} → 320（性能优化）")
            imgsz = 320
            half = True  # Jetson 默认开启 FP16

        self.model_path = model_path
        self.conf_threshold = conf_threshold
        self.iou_threshold = iou_threshold
        self.device = device
        self.imgsz = imgsz
        self.half = half
        self.model = YOLO(model_path)
        logger.info(f"YOLOv8模型已加载: {model_path}, imgsz={imgsz}, half={half}")

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
            half=self.half,  # FP16 半精度推理
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

    # 颜色名称映射：ColorDetector 颜色名 -> YOLO 类别名
    COLOR_MAP = {
        'red': 'red_block',
        'blue': 'blue_block',
        'green': 'green_block',
        'yellow': 'yellow_block',
        'black': 'black_block',
        'light_blue': 'light_blue_block'
    }

    def detect_center(self, img, color, min_area=0, max_area=float('inf')):
        """
        兼容 ColorDetector.detect() 接口，返回指定颜色目标的中心点坐标
        
        :param img: BGR图像 (H,W,3) 或路径
        :param color: 颜色名称，如 'red', 'blue', 'green'
        :param min_area: 最小面积阈值（用于过滤小目标）
        :param max_area: 最大面积阈值（用于过滤大目标）
        :return: (x, y) 中心点整数坐标；无目标返回None
        """
        if img is None or color not in self.COLOR_MAP:
            return None

        # 映射颜色名到 YOLO 类别名
        class_name = self.COLOR_MAP[color]
        
        # 获取类别ID
        class_id = None
        for cid, name in self.model.names.items():
            if name == class_name:
                class_id = cid
                break

        if class_id is None:
            logger.warning(f"类别 {class_name} 不在模型中")
            return None

        # 执行检测
        detections = self.detect(img, classes=[class_id])

        if not detections:
            return None

        # 选择面积最大的目标
        best_det = None
        best_area = 0
        for det in detections:
            w, h = det.size
            area = w * h
            if min_area <= area <= max_area and area > best_area:
                best_area = area
                best_det = det

        if best_det is None:
            return None

        return best_det.center
