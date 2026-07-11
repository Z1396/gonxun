#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Jetson Nano 自动配置文件
功能：
- 自动检测摄像头设备路径和索引
- 自动检测串口设备路径
- 自动检测 YOLO 模型路径
- 生成适配当前硬件的配置参数

使用方式：
  python config_jetson.py          # 仅检测并打印
  python config_jetson.py --apply  # 检测并写入 config.py
"""
import os
import sys
import subprocess
import glob
import logging

logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s')
logger = logging.getLogger(__name__)


def is_jetson():
    """检测是否在 Jetson 平台上运行"""
    try:
        with open('/etc/nv_tegra_release', 'r') as f:
            return True
    except FileNotFoundError:
        return False
    except Exception:
        return False


def detect_cameras():
    """自动检测摄像头设备，返回可用索引列表"""
    cameras = []

    # 方式1：通过 v4l2-ctl 检测（Linux）
    if sys.platform.startswith('linux'):
        try:
            result = subprocess.run(
                ['v4l2-ctl', '--list-devices'],
                capture_output=True, text=True, timeout=5
            )
            # 解析输出，提取 /dev/videoX
            lines = result.stdout.split('\n')
            for line in lines:
                if '/dev/video' in line:
                    # 提取设备路径
                    dev_path = line.strip().split()[0]
                    # 提取索引号
                    idx = int(dev_path.replace('/dev/video', ''))
                    cameras.append(idx)
        except (subprocess.TimeoutExpired, FileNotFoundError):
            pass

    # 方式2：通过 glob 扫描 /dev/video* 设备
    if not cameras and sys.platform.startswith('linux'):
        video_devices = sorted(glob.glob('/dev/video*'))
        for dev in video_devices:
            try:
                idx = int(dev.replace('/dev/video', ''))
                cameras.append(idx)
            except ValueError:
                continue

    # 方式3：Windows 平台
    if not cameras and sys.platform.startswith('win'):
        # Windows 摄像头索引通常从 0 开始
        import cv2
        for i in range(10):  # 尝试 0-9
            cap = cv2.VideoCapture(i, cv2.CAP_DSHOW)
            if cap.isOpened():
                cameras.append(i)
                cap.release()

    return sorted(set(cameras))


def detect_serial_ports():
    """自动检测串口设备路径"""
    ports = []

    # Linux/Jetson
    if sys.platform.startswith('linux'):
        # USB转串口
        ports.extend(glob.glob('/dev/ttyUSB*'))
        # Arduino/单片机
        ports.extend(glob.glob('/dev/ttyACM*'))
        # CH341芯片
        ports.extend(glob.glob('/dev/ttyCH341*'))
        # 传统串口
        ports.extend(glob.glob('/dev/ttyS*'))

    # Windows
    elif sys.platform.startswith('win'):
        import serial.tools.list_ports
        ports = [p.device for p in serial.tools.list_ports.comports()]

    return sorted(set(ports))


def detect_model_path():
    """自动检测 YOLO 模型路径"""
    # 可能的模型路径（按优先级）
    search_paths = [
        'yolo_pipeline/runs/detect/runs/material_detection-4/weights/best.pt',
        'yolo_pipeline/runs/detect/runs/material_detection-3/weights/best.pt',
        'yolo_pipeline/runs/detect/runs/material_detection-2/weights/best.pt',
        'yolo_pipeline/runs/detect/runs/material_detection/weights/best.pt',
        'runs/detect/runs/material_detection-4/weights/best.pt',
        'runs/detect/runs/material_detection-3/weights/best.pt',
        'yolov8n.pt',  # 预训练模型
    ]

    for path in search_paths:
        if os.path.exists(path):
            return path

    return 'yolov8n.pt'  # 默认


def get_system_info():
    """获取系统信息"""
    info = {}

    # Jetson 信息
    if is_jetson():
        try:
            with open('/etc/nv_tegra_release', 'r') as f:
                info['jetpack'] = f.read().strip()
        except Exception:
            pass

    # GPU 信息
    try:
        import torch
        info['pytorch_version'] = torch.__version__
        info['cuda_available'] = torch.cuda.is_available()
        if torch.cuda.is_available():
            info['gpu_name'] = torch.cuda.get_device_name(0)
            info['gpu_memory'] = f"{torch.cuda.get_device_properties(0).total_memory / 1024**3:.1f} GB"
    except ImportError:
        pass

    # 内存信息
    try:
        with open('/proc/meminfo', 'r') as f:
            for line in f:
                if 'MemTotal' in line:
                    mem_kb = int(line.split()[1])
                    info['total_memory'] = f"{mem_kb / 1024**2:.1f} GB"
                    break
    except Exception:
        pass

    return info


def generate_config():
    """生成配置字典"""
    config = {}

    # 检测硬件
    cameras = detect_cameras()
    serial_ports = detect_serial_ports()
    model_path = detect_model_path()
    system_info = get_system_info()

    # 摄像头配置
    config['CAMERA_MAIN_INDEX'] = cameras[0] if cameras else 0
    config['CAMERA_QR_INDEX'] = cameras[1] if len(cameras) > 1 else cameras[0] if cameras else 0
    config['CAMERA_MAIN_WIDTH'] = 640
    config['CAMERA_MAIN_HEIGHT'] = 480
    config['CAMERA_QR_WIDTH'] = 640
    config['CAMERA_QR_HEIGHT'] = 480

    # 串口配置
    config['SERIAL_PORT'] = serial_ports[0] if serial_ports else '/dev/ttyUSB0'
    config['SERIAL_BAUDRATE'] = 115200
    config['SERIAL_TIMEOUT'] = 0.05
    config['SERIAL_MOCK'] = len(serial_ports) == 0  # 无串口则模拟
    config['SERIAL_MOCK_CYCLE'] = True

    # YOLO 模型路径
    config['YOLO_MODEL_PATH'] = model_path

    # Jetson 性能优化参数
    if is_jetson():
        config['YOLO_IMGSZ'] = 320  # 降低推理尺寸
        config['YOLO_HALF'] = True  # FP16 半精度
    else:
        config['YOLO_IMGSZ'] = 640
        config['YOLO_HALF'] = False

    # 日志配置
    config['LOG_LEVEL'] = 'INFO'
    config['LOG_FORMAT'] = '%(asctime)s [%(levelname)s] %(message)s'

    # 颜色识别配置
    config['COLOR_MIN_AREA'] = 2000
    config['COLOR_DOCK_MIN_AREA'] = 3000

    # 卡尔曼滤波配置
    config['KALMAN_Q'] = 1e-5
    config['KALMAN_R'] = 1e-2

    return config, cameras, serial_ports, model_path, system_info


def print_detection_result(cameras, serial_ports, model_path, system_info):
    """打印检测结果"""
    print("=" * 60)
    print("Jetson Nano 硬件检测结果")
    print("=" * 60)

    # 系统信息
    print("\n【系统信息】")
    for key, value in system_info.items():
        print(f"  {key}: {value}")

    # 摄像头
    print(f"\n【摄像头设备】检测到 {len(cameras)} 个")
    if cameras:
        for i, idx in enumerate(cameras):
            print(f"  [{i}] /dev/video{idx}")
        print(f"  推荐: CAMERA_MAIN_INDEX = {cameras[0]}")
    else:
        print("  未检测到摄像头！")

    # 串口
    print(f"\n【串口设备】检测到 {len(serial_ports)} 个")
    if serial_ports:
        for i, port in enumerate(serial_ports):
            print(f"  [{i}] {port}")
        print(f"  推荐: SERIAL_PORT = '{serial_ports[0]}'")
    else:
        print("  未检测到串口设备，将使用模拟模式")

    # 模型
    print(f"\n【YOLO 模型】")
    print(f"  检测到: {model_path}")
    if os.path.exists(model_path):
        size_mb = os.path.getsize(model_path) / 1024 / 1024
        print(f"  文件大小: {size_mb:.2f} MB")

    print("\n" + "=" * 60)


def write_config_file(config, output_path='config.py'):
    """写入配置文件"""
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('''#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
全局配置文件（自动生成 - Jetson Nano）
生成时间: {timestamp}
"""
import os

# ========== 日志配置 ==========
LOG_LEVEL = "{LOG_LEVEL}"
LOG_FORMAT = "{LOG_FORMAT}"

# ========== 串口通信配置 ==========
SERIAL_PORT = '{SERIAL_PORT}'  # 自动检测
SERIAL_BAUDRATE = {SERIAL_BAUDRATE}
SERIAL_TIMEOUT = {SERIAL_TIMEOUT}
SERIAL_MOCK = {SERIAL_MOCK}  # {serial_mock_comment}
SERIAL_MOCK_CYCLE = {SERIAL_MOCK_CYCLE}

# ========== 摄像头配置 ==========
CAMERA_MAIN_INDEX = {CAMERA_MAIN_INDEX}  # 自动检测
CAMERA_QR_INDEX = {CAMERA_QR_INDEX}  # 自动检测
CAMERA_MAIN_WIDTH = {CAMERA_MAIN_WIDTH}
CAMERA_MAIN_HEIGHT = {CAMERA_MAIN_HEIGHT}
CAMERA_QR_WIDTH = {CAMERA_QR_WIDTH}
CAMERA_QR_HEIGHT = {CAMERA_QR_HEIGHT}

# ========== YOLO 模型配置 ==========
YOLO_MODEL_PATH = '{YOLO_MODEL_PATH}'  # 自动检测
YOLO_IMGSZ = {YOLO_IMGSZ}  # Jetson 优化：降低推理尺寸
YOLO_HALF = {YOLO_HALF}    # Jetson 优化：FP16 半精度

# ========== 颜色识别配置 ==========
COLOR_MIN_AREA = {COLOR_MIN_AREA}
COLOR_DOCK_MIN_AREA = {COLOR_DOCK_MIN_AREA}

# ========== 卡尔曼滤波配置 ==========
KALMAN_Q = {KALMAN_Q}
KALMAN_R = {KALMAN_R}

# ========== 仿真场地配置（单位：mm） ==========
FIELD_SIZE = 2400
PIXEL_PER_MM = 0.22
LANE_WIDTH = 400
LANE_CENTER = FIELD_SIZE // 2
LANE_START = LANE_CENTER - LANE_WIDTH // 2
LANE_END = LANE_CENTER + LANE_WIDTH // 2

# 启停区
START_ZONE_1 = (50, 50, 300, 300)
START_ZONE_2 = (2050, 50, 300, 300)

# 原料转盘
RAW_ZONE_CENTER = (1000, 1200)
RAW_ZONE_RADIUS = 150

# 加工/存放区
ROUGH_ZONE = (200, 1450, 580, 150)
TEMP_ZONE = (1620, 1450, 580, 150)

# 二维码板
QR_BOARD_POS = (2100, 1000)
'''.format(
    timestamp=__import__('datetime').datetime.now().strftime('%Y-%m-%d %H:%M:%S'),
    serial_mock_comment='无串口设备，使用模拟模式' if config['SERIAL_MOCK'] else '使用真实串口',
    **config
))

    logger.info(f"配置文件已写入: {output_path}")


def main():
    """主函数"""
    import argparse

    parser = argparse.ArgumentParser(description='Jetson Nano 自动配置工具')
    parser.add_argument('--apply', action='store_true', help='检测并写入配置文件')
    parser.add_argument('--output', default='config.py', help='配置文件输出路径')
    args = parser.parse_args()

    # 检测硬件
    config, cameras, serial_ports, model_path, system_info = generate_config()

    # 打印检测结果
    print_detection_result(cameras, serial_ports, model_path, system_info)

    # 写入配置文件（如果指定 --apply）
    if args.apply:
        write_config_file(config, args.output)
        print("\n配置已应用！请重启程序使用新配置。")
    else:
        print("\n提示: 使用 --apply 参数将配置写入 config.py")


if __name__ == '__main__':
    main()