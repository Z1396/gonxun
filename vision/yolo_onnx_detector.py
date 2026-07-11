"""
ONNX Runtime YOLOv8 检测器
适用于 Jetson Nano（PyTorch 不可用时）

使用方式：
1. 在 Windows 上将 YOLOv8 模型转换为 ONNX：
   from ultralytics import YOLO
   model = YOLO('best.pt')
   model.export(format='onnx', imgsz=320)

2. 在 Jetson Nano 上使用 ONNX Runtime 运行：
   detector = YOLOONNXDetector('best.onnx')
   results = detector.detect(img)
"""
import cv2
import numpy as np
import onnxruntime as ort
import logging

logger = logging.getLogger(__name__)


class YOLOONNXDetector:
    """ONNX Runtime YOLOv8 检测器"""

    # 颜色名称映射
    COLOR_MAP = {
        'red': 'red_block',
        'blue': 'blue_block',
        'green': 'green_block',
        'yellow': 'yellow_block',
        'black': 'black_block',
        'light_blue': 'light_blue_block'
    }

    def __init__(self, model_path='best.onnx', imgsz=320, conf_threshold=0.5):
        """
        :param model_path: ONNX 模型路径
        :param imgsz: 推理尺寸
        :param conf_threshold: 置信度阈值
        """
        self.imgsz = imgsz
        self.conf_threshold = conf_threshold

        # 初始化 ONNX Runtime
        try:
            # Jetson Nano 使用 CPU（CUDA provider 可能不可用）
            providers = ['CPUExecutionProvider']

            # 尝试使用 CUDA（如果可用）
            try:
                if 'CUDAExecutionProvider' in ort.get_available_providers():
                    providers.insert(0, 'CUDAExecutionProvider')
                    logger.info("使用 CUDA 加速")
            except Exception:
                pass

            self.session = ort.InferenceSession(model_path, providers=providers)
            logger.info(f"ONNX 模型加载成功: {model_path}")
            logger.info(f"使用执行器: {self.session.get_providers()}")

            # 获取输入输出信息
            self.input_name = self.session.get_inputs()[0].name
            self.output_name = self.session.get_outputs()[0].name

            # 类别名称（从模型元数据获取，或使用默认）
            try:
                metadata = self.session.get_modelmeta()
                if 'names' in metadata.custom_metadata_map:
                    names_str = metadata.custom_metadata_map['names']
                    self.names = eval(names_str)
                else:
                    self.names = {
                        0: 'red_block',
                        1: 'blue_block',
                        2: 'green_block',
                        3: 'yellow_block',
                        4: 'black_block',
                        5: 'light_blue_block'
                    }
            except Exception:
                self.names = {
                    0: 'red_block',
                    1: 'blue_block',
                    2: 'green_block',
                    3: 'yellow_block',
                    4: 'black_block',
                    5: 'light_blue_block'
                }

        except Exception as e:
            logger.error(f"ONNX 模型加载失败: {e}")
            raise

    def preprocess(self, img):
        """预处理图像"""
        # 缩放图像
        h, w = img.shape[:2]
        scale = min(self.imgsz / h, self.imgsz / w)
        new_h, new_w = int(h * scale), int(w * scale)

        # 缩放
        resized = cv2.resize(img, (new_w, new_h), interpolation=cv2.INTER_LINEAR)

        # 创建正方形图像（填充）
        padded = np.full((self.imgsz, self.imgsz, 3), 114, dtype=np.uint8)
        padded[:new_h, :new_w] = resized

        # 转换为 RGB
        rgb = cv2.cvtColor(padded, cv2.COLOR_BGR2RGB)

        # 归一化
        normalized = rgb.astype(np.float32) / 255.0

        # 转换为 (1, 3, H, W)
        tensor = normalized.transpose(2, 0, 1)
        tensor = np.expand_dims(tensor, axis=0)

        return tensor, scale, (new_w, new_h)

    def postprocess(self, outputs, scale, orig_shape, pad_size):
        """后处理输出"""
        # 输出形状：(1, num_boxes, 6) -> [x, y, w, h, conf, class_id]
        predictions = outputs[0]

        # 过滤低置信度检测
        detections = []
        for pred in predictions:
            x, y, w, h, conf, class_id = pred

            if conf >= self.conf_threshold:
                # 转换坐标到原图
                x1 = (x - w / 2) / scale
                y1 = (y - h / 2) / scale
                x2 = (x + w / 2) / scale
                y2 = (y + h / 2) / scale

                # 裁剪到原图范围
                orig_h, orig_w = orig_shape[:2]
                x1 = max(0, min(x1, orig_w))
                y1 = max(0, min(y1, orig_h))
                x2 = max(0, min(x2, orig_w))
                y2 = max(0, min(y2, orig_h))

                # 计算中心点
                center_x = (x1 + x2) / 2
                center_y = (y1 + y2) / 2

                detections.append({
                    'bbox': [x1, y1, x2, y2],
                    'center': (int(center_x), int(center_y)),
                    'confidence': float(conf),
                    'class_id': int(class_id),
                    'class_name': self.names.get(int(class_id), 'unknown')
                })

        return detections

    def detect(self, img):
        """
        执行检测
        :param img: BGR 图像
        :return: 检测结果列表
        """
        if img is None:
            return []

        # 预处理
        tensor, scale, pad_size = self.preprocess(img)

        # 推理
        outputs = self.session.run(
            [self.output_name],
            {self.input_name: tensor}
        )

        # 后处理
        detections = self.postprocess(outputs, scale, img.shape, pad_size)

        return detections

    def detect_center(self, img, color, min_area=0, max_area=float('inf')):
        """
        检测指定颜色的中心点（兼容 ColorDetector 接口）
        :param img: BGR 图像
        :param color: 颜色名称（red/blue/green/yellow/black/light_blue）
        :param min_area: 最小面积
        :param max_area: 最大面积
        :return: (x, y) 中心点坐标，或 None
        """
        if img is None or color not in self.COLOR_MAP:
            return None

        # 执行检测
        detections = self.detect(img)

        # 映射颜色名到类别名
        class_name = self.COLOR_MAP[color]

        # 找到最匹配的检测
        best_det = None
        best_conf = 0

        for det in detections:
            if det['class_name'] == class_name:
                # 计算面积
                x1, y1, x2, y2 = det['bbox']
                area = (x2 - x1) * (y2 - y1)

                # 面积过滤
                if min_area <= area <= max_area:
                    if det['confidence'] > best_conf:
                        best_conf = det['confidence']
                        best_det = det

        if best_det:
            return best_det['center']

        return None


def convert_yolo_to_onnx(pt_path='best.pt', onnx_path='best.onnx', imgsz=320):
    """
    在 Windows 上将 YOLOv8 PT 模型转换为 ONNX
    :param pt_path: PyTorch 模型路径
    :param onnx_path: ONNX 输出路径
    :param imgsz: 推理尺寸
    """
    try:
        from ultralytics import YOLO

        model = YOLO(pt_path)
        model.export(format='onnx', imgsz=imgsz, simplify=True)

        logger.info(f"ONNX 模型已导出: {onnx_path}")

    except ImportError:
        logger.error("ultralytics 未安装，无法转换模型")
        raise


# 使用示例
if __name__ == '__main__':
    import sys

    # 在 Windows 上转换模型
    if '--convert' in sys.argv:
        convert_yolo_to_onnx('yolo_pipeline/runs/detect/runs/material_detection-4/weights/best.pt')

    # 在 Jetson Nano 上测试模型
    else:
        logging.basicConfig(level=logging.INFO)

        # 加载模型
        detector = YOLOONNXDetector('best.onnx')

        # 测试摄像头
        cap = cv2.VideoCapture(0)
        while True:
            ret, frame = cap.read()
            if not ret:
                break

            # 检测
            detections = detector.detect(frame)

            # 绘制结果
            for det in detections:
                x1, y1, x2, y2 = det['bbox']
                cv2.rectangle(frame, (int(x1), int(y1)), (int(x2), int(y2)), (0, 255, 0), 2)
                cv2.putText(frame, f"{det['class_name']} {det['confidence']:.2f}",
                           (int(x1), int(y1) - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.5, (0, 255, 0), 2)

            cv2.imshow('ONNX Detection', frame)
            if cv2.waitKey(1) & 0xFF == ord('q'):
                break

        cap.release()
        cv2.destroyAllWindows()