"""
视觉系统子包
工创赛2025智能物流搬运 - 视觉处理模块集合

模块结构：
- color_detector:   颜色识别 (6种物料颜色)
- ring_detector:    圆环检测 (3环定位 + 6环评分)
- qr_detector:      二维码识别 + 任务码解析
- kalman_filter:    卡尔曼滤波
- serial_comm:      串口通信 (真实 + 模拟)
- camera_manager:   摄像头管理 (主 + 扫码)
- task_display:     任务码显示装置
- obstacle_detector: 障碍物检测
"""
from .color_detector import (
    ColorDetector,
    COLOR_DIST,
    COLOR_ID_MAP,
    get_color_by_id
)
from .ring_detector import (
    ThreeRingDetector,
    SixRingDetector,
    RING_SCORES,
    calc_placement_score
)
from .qr_detector import QRDetector, TaskCodeParser
from .kalman_filter import KalmanFilter
from .serial_comm import (
    SerialComm,
    MODE_IDLE, MODE_COLOR, MODE_RING, MODE_DOCK, MODE_QR,
    CMD_COLOR, CMD_RING, CMD_DOCK, CMD_QR,
    FRAME_HEADER, FRAME_TAIL
)
from .camera_manager import CameraManager
from .task_display import TaskDisplay
from .obstacle_detector import ObstacleDetector
from .system import VisionSystem
from .utils import FPSCounter, check_gui_available, generate_test_frame

# YOLOv8 + 传统CV 融合检测模块（依赖ultralytics，未安装时导入不报错）
from .yolo_detector import YOLO_AVAILABLE
from .region_extractor import RegionExtractor
from .precise_locator import PreciseLocator, PreciseResult
from .fusion_detector import FusionDetector, FusionResult, FusionDetectorWithTiming
from .visualization import Visualizer

# 物料识别系统（YOLOv8 + 颜色检测融合）
from .material_recognizer import MaterialRecognizer, MaterialResult
from .material_visualizer import MaterialVisualizer

__all__ = [
    # 类
    'ColorDetector', 'ThreeRingDetector', 'SixRingDetector',
    'QRDetector', 'TaskCodeParser', 'KalmanFilter',
    'SerialComm', 'CameraManager', 'TaskDisplay', 'ObstacleDetector',
    'VisionSystem', 'FPSCounter',
    # 融合检测
    'RegionExtractor', 'PreciseLocator', 'PreciseResult',
    'FusionDetector', 'FusionResult', 'FusionDetectorWithTiming',
    'Visualizer',
    # 物料识别
    'MaterialRecognizer', 'MaterialResult', 'MaterialVisualizer',
    # 常量
    'COLOR_DIST', 'COLOR_ID_MAP', 'RING_SCORES',
    'MODE_IDLE', 'MODE_COLOR', 'MODE_RING', 'MODE_DOCK', 'MODE_QR',
    'CMD_COLOR', 'CMD_RING', 'CMD_DOCK', 'CMD_QR',
    'FRAME_HEADER', 'FRAME_TAIL',
    'YOLO_AVAILABLE',
    # 函数
    'get_color_by_id', 'calc_placement_score',
    'check_gui_available', 'generate_test_frame'
]
