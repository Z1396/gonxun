#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
物料识别系统主程序入口
YOLOv8 + 颜色检测融合

使用方式:
  python material_recognition_system.py                          # 摄像头实时识别
  python material_recognition_system.py --image path/to/img.jpg  # 单张图像
  python material_recognition_system.py --video path/to/vid.mp4  # 视频文件
  python material_recognition_system.py --camera 1               # 指定摄像头
  python material_recognition_system.py --model yolov8s.pt       # 指定模型
  python material_recognition_system.py --conf 0.3               # 置信度阈值
  python material_recognition_system.py --no-display             # 无头模式
  python material_recognition_system.py --save result.jpg        # 保存结果
"""
import argparse
import logging
import sys
import time

import cv2
import numpy as np

logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s')
logger = logging.getLogger(__name__)

from vision import (
    MaterialRecognizer, MaterialVisualizer,
    YOLO_AVAILABLE, check_gui_available, FPSCounter
)


def parse_args():
    """解析命令行参数"""
    p = argparse.ArgumentParser(description='物料识别系统：YOLOv8 + 颜色检测')
    p.add_argument('--model', default='yolov8n.pt', help='YOLOv8模型路径')
    p.add_argument('--image', help='输入图像路径')
    p.add_argument('--video', help='输入视频路径')
    p.add_argument('--camera', type=int, default=0, help='摄像头索引')
    p.add_argument('--conf', type=float, default=0.5, help='YOLO置信度阈值')
    p.add_argument('--classes', type=int, nargs='+', help='只检测指定类别ID')
    p.add_argument('--no-display', action='store_true', help='不显示窗口(无头模式)')
    p.add_argument('--save', help='保存结果到路径')
    p.add_argument('--device', help='推理设备 cpu/cuda:0')
    p.add_argument('--imgsz', type=int, default=640, help='推理图像尺寸')
    return p.parse_args()


def run_image(recognizer, visualizer, img_path, classes, save_path, show):
    """单张图像识别"""
    img = cv2.imread(img_path)
    if img is None:
        logger.error(f"无法读取图像: {img_path}")
        return 1

    logger.info(f"图像尺寸: {img.shape[1]}x{img.shape[0]}")
    results = recognizer.recognize(img, classes=classes)

    logger.info(f"识别到 {len(results)} 个物料:")
    for i, r in enumerate(results):
        logger.info(f"  [{i}] {r}")

    out = visualizer.draw_results(img, results)
    if save_path:
        cv2.imwrite(save_path, out)
        logger.info(f"结果已保存: {save_path}")

    if show:
        cv2.imshow('Material Recognition', out)
        cv2.waitKey(0)
        cv2.destroyAllWindows()
    return 0


def run_stream(recognizer, visualizer, source, classes, save_path, show):
    """视频流识别（摄像头或视频文件）"""
    cap = cv2.VideoCapture(source)
    if not cap.isOpened():
        logger.error(f"无法打开视频源: {source}")
        return 1

    fps_counter = FPSCounter(update_interval=10)
    writer = None
    if save_path:
        w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        fourcc = cv2.VideoWriter_fourcc(*'mp4v')
        writer = cv2.VideoWriter(save_path, fourcc, 25, (w, h))

    logger.info("实时识别已启动，按 'q' 退出")
    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                break

            results = recognizer.recognize(frame, classes=classes)
            out = visualizer.draw_results(frame, results)

            fps = fps_counter.tick()
            out = visualizer.draw_fps(out, fps)
            out = visualizer.draw_summary(out, results)

            if writer:
                writer.write(out)
            if show:
                cv2.imshow('Material Recognition', out)
                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
    except KeyboardInterrupt:
        logger.info("用户中断")
    finally:
        cap.release()
        if writer:
            writer.release()
        if show:
            cv2.destroyAllWindows()
    return 0


def main():
    args = parse_args()

    if not YOLO_AVAILABLE:
        logger.error("ultralytics未安装。请执行: pip install ultralytics")
        return 1

    logger.info(f"初始化物料识别器 (model={args.model})")
    try:
        recognizer = MaterialRecognizer(
            yolo_model=args.model, conf_threshold=args.conf,
            device=args.device, imgsz=args.imgsz
        )
        recognizer.warmup()
    except Exception as e:
        logger.error(f"识别器初始化失败: {e}", exc_info=True)
        return 1

    visualizer = MaterialVisualizer(show_conf=True, show_color_conf=True)
    show = not args.no_display and check_gui_available()

    if args.image:
        return run_image(recognizer, visualizer, args.image,
                         args.classes, args.save, show)
    elif args.video:
        return run_stream(recognizer, visualizer, args.video,
                          args.classes, args.save, show)
    else:
        return run_stream(recognizer, visualizer, args.camera,
                          args.classes, args.save, show)


if __name__ == "__main__":
    sys.exit(main())
