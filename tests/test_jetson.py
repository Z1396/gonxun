#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Jetson Nano 系统测试脚本
功能：
1. 检测并测试摄像头设备
2. 检测并测试串口通信
3. 测试 YOLO 模型加载和推理
4. 测试 GPU/CUDA 环境
5. 输出完整诊断报告

使用方式:
  python tests/test_jetson.py          # 运行全部测试
  python tests/test_jetson.py --camera # 仅测试摄像头
  python tests/test_jetson.py --serial # 仅测试串口
  python tests/test_jetson.py --model  # 仅测试模型
"""
import sys
import os
import time
import argparse
import logging

# 添加项目根目录到路径
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s')
logger = logging.getLogger(__name__)


def test_camera():
    """测试摄像头设备"""
    print("\n" + "=" * 60)
    print("【摄像头测试】")
    print("=" * 60)

    try:
        import cv2

        # 检测摄像头数量
        cameras = []
        for i in range(10):
            cap = cv2.VideoCapture(i, cv2.CAP_V4L2 if sys.platform.startswith('linux') else cv2.CAP_DSHOW)
            if cap.isOpened():
                cameras.append(i)
                cap.release()

        if not cameras:
            print("❌ 未检测到摄像头设备")
            return False

        print(f"✅ 检测到 {len(cameras)} 个摄像头: {cameras}")

        # 测试第一个摄像头
        print(f"\n测试摄像头 {cameras[0]}...")
        cap = cv2.VideoCapture(cameras[0], cv2.CAP_V4L2 if sys.platform.startswith('linux') else cv2.CAP_DSHOW)

        if not cap.isOpened():
            print(f"❌ 无法打开摄像头 {cameras[0]}")
            return False

        # 设置分辨率
        cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
        cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

        # 读取10帧测试
        success_count = 0
        start_time = time.time()

        for _ in range(10):
            ret, frame = cap.read()
            if ret:
                success_count += 1

        elapsed = time.time() - start_time
        fps = success_count / elapsed if elapsed > 0 else 0

        cap.release()

        if success_count >= 8:
            print(f"✅ 摄像头读取成功: {success_count}/10 帧")
            print(f"✅ FPS: {fps:.1f}")
            return True
        else:
            print(f"❌ 摄像头读取不稳定: {success_count}/10 帧")
            return False

    except ImportError:
        print("❌ OpenCV 未安装")
        return False
    except Exception as e:
        print(f"❌ 测试失败: {e}")
        return False


def test_serial():
    """测试串口通信"""
    print("\n" + "=" * 60)
    print("【串口测试】")
    print("=" * 60)

    try:
        import serial
        import serial.tools.list_ports
        import glob

        # 检测串口设备
        ports = []

        # Linux
        if sys.platform.startswith('linux'):
            ports.extend(glob.glob('/dev/ttyUSB*'))
            ports.extend(glob.glob('/dev/ttyACM*'))
            ports.extend(glob.glob('/dev/ttyCH341*'))
        # Windows
        elif sys.platform.startswith('win'):
            ports = [p.device for p in serial.tools.list_ports.comports()]

        ports = sorted(set(ports))

        if not ports:
            print("⚠️  未检测到串口设备（将使用模拟模式）")
            return True  # 无串口不算失败

        print(f"✅ 检测到 {len(ports)} 个串口设备:")
        for port in ports:
            print(f"   - {port}")

        # 尝试打开第一个串口
        print(f"\n测试串口 {ports[0]}...")
        try:
            ser = serial.Serial(
                port=ports[0],
                baudrate=115200,
                timeout=1.0
            )

            if ser.is_open:
                print(f"✅ 串口打开成功: {ports[0]}")
                ser.close()
                return True
            else:
                print(f"❌ 串口打开失败")
                return False

        except serial.SerialException as e:
            print(f"❌ 串口打开失败: {e}")
            print("   可能原因：")
            print("   1. 权限不足（运行: sudo usermod -aG dialout $USER）")
            print("   2. 设备被其他程序占用")
            return False

    except ImportError:
        print("❌ pyserial 未安装")
        return False
    except Exception as e:
        print(f"❌ 测试失败: {e}")
        return False


def test_model():
    """测试 YOLO 模型"""
    print("\n" + "=" * 60)
    print("【YOLO 模型测试】")
    print("=" * 60)

    try:
        import torch
        import numpy as np

        # 测试 PyTorch
        print(f"PyTorch 版本: {torch.__version__}")
        print(f"CUDA 可用: {torch.cuda.is_available()}")

        if torch.cuda.is_available():
            print(f"GPU 型号: {torch.cuda.get_device_name(0)}")
            print(f"GPU 内存: {torch.cuda.get_device_properties(0).total_memory / 1024**3:.1f} GB")

        # 测试 ultralytics
        try:
            from ultralytics import YOLO
            print("✅ ultralytics 已安装")
        except ImportError:
            print("❌ ultralytics 未安装")
            return False

        # 查找模型文件
        model_paths = [
            'yolo_pipeline/runs/detect/runs/material_detection-4/weights/best.pt',
            'yolov8n.pt'
        ]

        model_path = None
        for path in model_paths:
            if os.path.exists(path):
                model_path = path
                break

        if not model_path:
            print("⚠️  未找到模型文件，将使用预训练模型")
            model_path = 'yolov8n.pt'

        print(f"\n加载模型: {model_path}")

        # 加载模型
        model = YOLO(model_path)

        # 测试推理
        print("测试推理...")
        dummy_img = np.zeros((640, 640, 3), dtype=np.uint8)

        # 预热
        model.predict(source=dummy_img, imgsz=320, verbose=False)

        # FPS 测试
        start_time = time.time()
        for _ in range(10):
            model.predict(source=dummy_img, imgsz=320, verbose=False)
        elapsed = time.time() - start_time

        fps = 10 / elapsed
        print(f"✅ 推理成功")
        print(f"✅ FPS: {fps:.1f}")

        # 模型信息
        print(f"\n模型信息:")
        print(f"  类别数: {len(model.names)}")
        print(f"  类别: {list(model.names.values())}")

        return True

    except ImportError as e:
        print(f"❌ 依赖缺失: {e}")
        return False
    except Exception as e:
        print(f"❌ 测试失败: {e}")
        return False


def test_system_info():
    """打印系统信息"""
    print("\n" + "=" * 60)
    print("【系统信息】")
    print("=" * 60)

    # 检测 Jetson
    try:
        with open('/etc/nv_tegra_release', 'r') as f:
            print(f"Jetson 设备: {f.read().strip()}")
    except FileNotFoundError:
        print("非 Jetson 平台")

    # 内存信息
    try:
        with open('/proc/meminfo', 'r') as f:
            for line in f:
                if 'MemTotal' in line:
                    mem_kb = int(line.split()[1])
                    print(f"总内存: {mem_kb / 1024**2:.1f} GB")
                    break
    except Exception:
        pass

    # CPU 信息
    try:
        with open('/proc/cpuinfo', 'r') as f:
            lines = f.readlines()
            for line in lines:
                if 'model name' in line.lower():
                    print(f"CPU: {line.split(':')[1].strip()}")
                    break
    except Exception:
        pass


def main():
    """主测试函数"""
    parser = argparse.ArgumentParser(description='Jetson Nano 系统测试')
    parser.add_argument('--camera', action='store_true', help='仅测试摄像头')
    parser.add_argument('--serial', action='store_true', help='仅测试串口')
    parser.add_argument('--model', action='store_true', help='仅测试模型')
    args = parser.parse_args()

    print("\n" + "=" * 60)
    print("  Jetson Nano 系统诊断测试")
    print("=" * 60)

    results = {}

    # 系统信息
    test_system_info()

    # 根据参数选择测试
    if args.camera:
        results['camera'] = test_camera()
    elif args.serial:
        results['serial'] = test_serial()
    elif args.model:
        results['model'] = test_model()
    else:
        # 运行全部测试
        results['camera'] = test_camera()
        results['serial'] = test_serial()
        results['model'] = test_model()

    # 输出总结
    print("\n" + "=" * 60)
    print("【测试结果汇总】")
    print("=" * 60)

    all_passed = True
    for name, passed in results.items():
        status = "✅ 通过" if passed else "❌ 失败"
        print(f"  {name}: {status}")
        if not passed:
            all_passed = False

    print("=" * 60)

    if all_passed:
        print("\n✅ 所有测试通过！系统运行正常")
        return 0
    else:
        print("\n⚠️  部分测试未通过，请检查配置")
        return 1


if __name__ == '__main__':
    sys.exit(main())