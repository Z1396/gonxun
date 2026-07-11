"""
TensorRT YOLOv8 检测器
适用于 Jetson Nano（性能最佳）

工作流程：
1. ONNX → TensorRT Engine（在 Jetson 上转换）
2. 使用 TensorRT Python API 执行推理

使用方式：
# 1. 转换 ONNX 为 TensorRT Engine
python3 yolo_tensorrt_detector.py --convert best.onnx

# 2. 使用 TensorRT Engine 检测
detector = YOLOTensorRTDetector('best.engine')
results = detector.detect(img)
"""
import cv2
import numpy as np
import tensorrt as trt
import logging
import os
import argparse

logger = logging.getLogger(__name__)
TRT_LOGGER = trt.Logger(trt.Logger.WARNING)


class YOLOTensorRTDetector:
    """TensorRT YOLOv8 检测器"""

    # 颜色名称映射
    COLOR_MAP = {
        'red': 'red_block',
        'blue': 'blue_block',
        'green': 'green_block',
        'yellow': 'yellow_block',
        'black': 'black_block',
        'light_blue': 'light_blue_block'
    }

    def __init__(self, engine_path='best.engine', imgsz=320, conf_threshold=0.5):
        """
        :param engine_path: TensorRT Engine 路径
        :param imgsz: 推理尺寸
        :param conf_threshold: 置信度阈值
        """
        self.imgsz = imgsz
        self.conf_threshold = conf_threshold

        # 加载 TensorRT Engine
        try:
            logger.info(f"加载 TensorRT Engine: {engine_path}")

            # 读取 engine 文件
            with open(engine_path, 'rb') as f:
                engine_data = f.read()

            # 创建 runtime 和 engine
            self.runtime = trt.Runtime(TRT_LOGGER)
            self.engine = self.runtime.deserialize_cuda_engine(engine_data)
            self.context = self.engine.create_execution_context()

            # 获取输入输出信息
            self.input_name = None
            self.output_name = None
            for binding in self.engine:
                if self.engine.binding_is_input(binding):
                    self.input_name = binding
                else:
                    self.output_name = binding

            logger.info(f"输入: {self.input_name}, 输出: {self.output_name}")

            # 类别名称
            self.names = {
                0: 'red_block',
                1: 'blue_block',
                2: 'green_block',
                3: 'yellow_block',
                4: 'black_block',
                5: 'light_blue_block'
            }

            logger.info("TensorRT Engine 加载成功")

        except Exception as e:
            logger.error(f"TensorRT Engine 加载失败: {e}")
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

        # 创建输入输出缓冲区
        input_buffer = tensor
        output_buffer = np.empty((1, 100, 6), dtype=np.float32)  # 根据模型调整

        # 执行推理
        bindings = [int(input_buffer.ctypes.data), int(output_buffer.ctypes.data)]
        self.context.execute_v2(bindings)

        # 后处理
        detections = self.postprocess(output_buffer, scale, img.shape, pad_size)

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


def convert_onnx_to_engine(onnx_path='best.onnx', engine_path='best.engine', imgsz=320):
    """
    将 ONNX 模型转换为 TensorRT Engine
    :param onnx_path: ONNX 模型路径
    :param engine_path: TensorRT Engine 输出路径
    :param imgsz: 推理尺寸
    """
    logger.info(f"转换 ONNX → TensorRT: {onnx_path} → {engine_path}")

    # 创建 builder
    builder = trt.Builder(TRT_LOGGER)
    network = builder.create_network(1 << int(trt.NetworkDefinitionCreationFlag.EXPLICIT_BATCH))
    parser = trt.OnnxParser(network, TRT_LOGGER)

    # 解析 ONNX 模型
    with open(onnx_path, 'rb') as f:
        if not parser.parse(f.read()):
            logger.error("ONNX 解析失败")
            for error in range(parser.num_errors):
                logger.error(parser.get_error(error))
            return False

    # 配置 builder
    config = builder.create_builder_config()
    config.max_workspace_size = 1 << 30  # 1 GB

    # 设置输入形状
    input_tensor = network.get_input(0)
    input_tensor.shape = (1, 3, imgsz, imgsz)

    # 构建 engine
    logger.info("构建 TensorRT Engine...")
    engine = builder.build_engine(network, config)

    if engine is None:
        logger.error("Engine 构建失败")
        return False

    # 保存 engine
    with open(engine_path, 'wb') as f:
        f.write(engine.serialize())

    logger.info(f"TensorRT Engine 已保存: {engine_path}")
    logger.info(f"文件大小: {os.path.getsize(engine_path) / 1024 / 1024:.2f} MB")

    return True


# 使用示例
if __name__ == '__main__':
    parser = argparse.ArgumentParser(description='TensorRT YOLOv8 检测器')
    parser.add_argument('--convert', action='store_true', help='转换 ONNX 为 TensorRT Engine')
    parser.add_argument('--onnx', default='best.onnx', help='ONNX 模型路径')
    parser.add_argument('--engine', default='best.engine', help='TensorRT Engine 路径')
    parser.add_argument('--test', action='store_true', help='测试 Engine')
    args = parser.parse_args()

    logging.basicConfig(level=logging.INFO)

    # 转换模式
    if args.convert:
        if not os.path.exists(args.onnx):
            logger.error(f"ONNX 文件不存在: {args.onnx}")
        else:
            convert_onnx_to_engine(args.onnx, args.engine)

    # 测试模式
    elif args.test:
        if not os.path.exists(args.engine):
            logger.error(f"Engine 文件不存在: {args.engine}")
        else:
            detector = YOLOTensorRTDetector(args.engine)

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

                cv2.imshow('TensorRT Detection', frame)
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break

            cap.release()
            cv2.destroyAllWindows()