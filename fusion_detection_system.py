#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
YOLOv8 + 传统CV 两阶段目标检测与精确定位系统

工作流程：
1. YOLOv8 网络对输入图像/视频流进行目标初步检测 → 边界框
2. 基于边界框裁剪 ROI 区域
3. 传统CV算法 (轮廓/边缘/模板/角点) 对 ROI 做精确定位 → 亚像素中心
4. 可视化输出：YOLO框 + 精确中心 + 偏移向量

使用方式:
  python fusion_detection_system.py                          # 摄像头实时检测
  python fusion_detection_system.py --image path/to/img.jpg  # 单张图像
  python fusion_detection_system.py --video path/to/vid.mp4  # 视频文件
  python fusion_detection_system.py --method contour         # 指定定位方法
  python fusion_detection_system.py --no-precise             # 仅YOLO不精定位

依赖:
  pip install ultralytics  # YOLOv8
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
    FusionDetector, FusionDetectorWithTiming, Visualizer,
    YOLO_AVAILABLE, check_gui_available, FPSCounter
)


def parse_args():
    """解析命令行参数"""
    p = argparse.ArgumentParser(description='YOLOv8 + 传统CV 两阶段融合检测系统')
    p.add_argument('--model', default='yolov8n.pt', help='YOLOv8模型路径')
    p.add_argument('--image', help='输入图像路径')
    p.add_argument('--video', help='输入视频路径')
    p.add_argument('--camera', type=int, default=0, help='摄像头索引')
    p.add_argument('--method', default='contour',
                   choices=['contour', 'edge', 'template', 'corner'],
                   help='精确定位方法')
    p.add_argument('--conf', type=float, default=0.5, help='YOLO置信度阈值')
    p.add_argument('--classes', type=int, nargs='+', help='只检测指定类别ID')
    p.add_argument('--no-precise', action='store_true', help='禁用第二阶段精确定位')
    p.add_argument('--no-display', action='store_true', help='不显示窗口(无头模式)')
    p.add_argument('--save', help='保存结果到路径')
    p.add_argument('--device', help='推理设备 cpu/cuda:0')
    return p.parse_args()


def run_image(detector, visualizer, img_path, classes, save_path, show):
    """单张图像检测"""
    img = cv2.imread(img_path)
    if img is None:
        logger.error(f"无法读取图像: {img_path}")
        return 1

    logger.info(f"图像尺寸: {img.shape[1]}x{img.shape[0]}")
    results = detector.detect(img, classes=classes)

    logger.info(f"检测到 {len(results)} 个目标:")
    for i, r in enumerate(results):
        logger.info(f"  [{i}] {r}")

    out = visualizer.draw_fusion_results(img, results)
    if save_path:
        cv2.imwrite(save_path, out)
        logger.info(f"结果已保存: {save_path}")

    if show:
        cv2.imshow('Fusion Detection', out)
        cv2.waitKey(0)
        cv2.destroyAllWindows()
    return 0


def run_stream(detector, visualizer, source, classes, save_path, show):
    """视频流检测（摄像头或视频文件）"""
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

    logger.info("实时检测已启动，按 'q' 退出")
    try:
        while True:
            ret, frame = cap.read()
            if not ret:
                break

            results = detector.detect(frame, classes=classes)
            out = visualizer.draw_fusion_results(frame, results)

            fps = fps_counter.tick()
            out = visualizer.draw_fps(out, fps)

            # 耗时信息
            if hasattr(detector, 'last_total_ms'):
                out = visualizer.draw_stage_info(
                    out, getattr(detector, 'last_yolo_ms', 0),
                    getattr(detector, 'last_precise_ms', 0),
                    detector.last_total_ms
                )

            if writer:
                writer.write(out)
            if show:
                cv2.imshow('Fusion Detection', out)
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

    logger.info(f"初始化融合检测器 (method={args.method})")
    try:
        detector = FusionDetector(
            yolo_model=args.model, conf_threshold=args.conf,
            locate_method=args.method, device=args.device,
            enable_precise=not args.no_precise
        )
        detector = FusionDetectorWithTiming(detector)
        detector.warmup()
    except Exception as e:
        logger.error(f"检测器初始化失败: {e}", exc_info=True)
        return 1

    visualizer = Visualizer(show_offset=True, show_conf=True)
    show = not args.no_display and check_gui_available()

    if args.image:
        return run_image(detector, visualizer, args.image,
                         args.classes, args.save, show)
    elif args.video:
        return run_stream(detector, visualizer, args.video,
                          args.classes, args.save, show)
    else:
        return run_stream(detector, visualizer, args.camera,
                          args.classes, args.save, show)


if __name__ == "__main__":
    sys.exit(main())
