#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
推理部署脚本
功能：
1. 加载训练好的模型
2. 支持多种输入源（摄像头、图片、视频、目录）
3. 实时检测与可视化
4. 结果保存（图片/视频/JSON）
5. 性能统计（FPS、延迟）

使用方式:
  python inference.py                                    # 摄像头实时推理
  python inference.py --source image.jpg                 # 单张图片
  python inference.py --source video.mp4                 # 视频文件
  python inference.py --source ./test_images/            # 图片目录
  python inference.py --model runs/detect/exp/weights/best.pt
  python inference.py --conf 0.3                         # 调整置信度
  python inference.py --save-json                        # 保存JSON结果
  python inference.py --export onnx                      # 导出模型
"""
import argparse
import json
import logging
import sys
import time
from pathlib import Path

import cv2
import numpy as np

from config_loader import PipelineConfig

logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s')
logger = logging.getLogger(__name__)

try:
    from ultralytics import YOLO
    YOLO_AVAILABLE = True
except ImportError:
    YOLO_AVAILABLE = False
    logger.error("ultralytics未安装。请执行: pip install ultralytics")


class InferenceEngine:
    """推理引擎"""

    def __init__(self, config: PipelineConfig):
        self.config = config
        self.model = None
        self.fps_counter = []

    def load_model(self) -> bool:
        """加载模型"""
        if not YOLO_AVAILABLE:
            logger.error("ultralytics未安装")
            return False

        model_path = self._get_model_path()
        if not model_path:
            logger.error("未找到模型文件")
            return False

        logger.info(f"加载模型: {model_path}")
        self.model = YOLO(model_path)

        # 预热
        dummy = np.zeros((640, 640, 3), dtype=np.uint8)
        self.model.predict(dummy, verbose=False)
        logger.info("模型预热完成")
        return True

    def run(self, save_json=False) -> bool:
        """执行推理"""
        if not self.model:
            if not self.load_model():
                return False

        source = self.config.inference.source
        logger.info(f"输入源: {source}")

        # 判断输入类型
        if source.isdigit():
            return self._run_camera(int(source), save_json)
        elif Path(source).is_dir():
            return self._run_directory(source, save_json)
        elif Path(source).is_file():
            ext = Path(source).suffix.lower()
            if ext in {'.mp4', '.avi', '.mov', '.mkv', '.wmv'}:
                return self._run_video(source, save_json)
            else:
                return self._run_image(source, save_json)
        else:
            logger.error(f"无效输入源: {source}")
            return False

    def _run_camera(self, camera_id: int, save_json: bool) -> bool:
        """摄像头实时推理"""
        cap = cv2.VideoCapture(camera_id)
        if not cap.isOpened():
            logger.error(f"无法打开摄像头 {camera_id}")
            return False

        logger.info("摄像头已打开，按 'q' 退出")
        results_list = []

        try:
            while True:
                ret, frame = cap.read()
                if not ret:
                    break

                t0 = time.time()
                results = self.model.predict(
                    source=frame, conf=self.config.inference.conf_threshold,
                    verbose=False
                )
                elapsed = (time.time() - t0) * 1000

                # 绘制结果
                annotated = results[0].plot()

                # FPS
                self.fps_counter.append(elapsed)
                if len(self.fps_counter) > 30:
                    self.fps_counter.pop(0)
                avg_ms = np.mean(self.fps_counter)
                fps = 1000 / avg_ms if avg_ms > 0 else 0

                cv2.putText(annotated, f"FPS: {fps:.1f} | {elapsed:.0f}ms",
                            (10, 25), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

                # 检测数量
                n_boxes = len(results[0].boxes) if results[0].boxes else 0
                cv2.putText(annotated, f"Detections: {n_boxes}",
                            (10, 50), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

                if self.config.inference.show:
                    cv2.imshow('Inference', annotated)
                    if cv2.waitKey(1) & 0xFF == ord('q'):
                        break

                if save_json:
                    results_list.append(self._result_to_dict(results[0]))

        except KeyboardInterrupt:
            logger.info("用户中断")
        finally:
            cap.release()
            if self.config.inference.show:
                cv2.destroyAllWindows()

        if save_json and results_list:
            self._save_json(results_list)

        return True

    def _run_image(self, img_path: str, save_json: bool) -> bool:
        """单张图片推理"""
        img = cv2.imread(img_path)
        if img is None:
            logger.error(f"无法读取图片: {img_path}")
            return False

        t0 = time.time()
        results = self.model.predict(
            source=img, conf=self.config.inference.conf_threshold, verbose=False
        )
        elapsed = (time.time() - t0) * 1000

        annotated = results[0].plot()
        cv2.putText(annotated, f"{elapsed:.0f}ms", (10, 25),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

        # 保存结果
        if self.config.inference.save:
            out_path = f"inference_result_{Path(img_path).name}"
            cv2.imwrite(out_path, annotated)
            logger.info(f"结果已保存: {out_path}")

        if self.config.inference.show:
            cv2.imshow('Inference', annotated)
            cv2.waitKey(0)
            cv2.destroyAllWindows()

        if save_json:
            self._save_json([self._result_to_dict(results[0])])

        # 打印检测结果
        self._print_detections(results[0])

        return True

    def _run_video(self, video_path: str, save_json: bool) -> bool:
        """视频文件推理"""
        cap = cv2.VideoCapture(video_path)
        if not cap.isOpened():
            logger.error(f"无法打开视频: {video_path}")
            return False

        w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        fps = cap.get(cv2.CAP_PROP_FPS)

        out_path = f"inference_result_{Path(video_path).name}"
        fourcc = cv2.VideoWriter_fourcc(*'mp4v')
        writer = cv2.VideoWriter(out_path, fourcc, fps, (w, h))

        results_list = []
        frame_count = 0

        logger.info(f"视频: {w}x{h} @ {fps:.1f}fps")
        try:
            while True:
                ret, frame = cap.read()
                if not ret:
                    break

                results = self.model.predict(
                    source=frame, conf=self.config.inference.conf_threshold,
                    verbose=False
                )
                annotated = results[0].plot()
                writer.write(annotated)

                if save_json:
                    results_list.append(self._result_to_dict(results[0]))

                frame_count += 1
                if frame_count % 30 == 0:
                    logger.info(f"已处理 {frame_count} 帧")

                if self.config.inference.show:
                    cv2.imshow('Inference', annotated)
                    if cv2.waitKey(1) & 0xFF == ord('q'):
                        break
        except KeyboardInterrupt:
            pass
        finally:
            cap.release()
            writer.release()
            if self.config.inference.show:
                cv2.destroyAllWindows()

        logger.info(f"结果已保存: {out_path} ({frame_count} 帧)")

        if save_json and results_list:
            self._save_json(results_list)

        return True

    def _run_directory(self, dir_path: str, save_json: bool) -> bool:
        """目录批量推理"""
        img_dir = Path(dir_path)
        img_exts = {'.jpg', '.jpeg', '.png', '.bmp', '.webp'}
        images = [f for f in img_dir.iterdir() if f.suffix.lower() in img_exts]

        if not images:
            logger.error(f"目录中无图片文件: {dir_path}")
            return False

        logger.info(f"找到 {len(images)} 张图片")
        out_dir = img_dir / "results"
        out_dir.mkdir(exist_ok=True)

        results_list = []
        for i, img_path in enumerate(images):
            img = cv2.imread(str(img_path))
            if img is None:
                continue

            t0 = time.time()
            results = self.model.predict(
                source=img, conf=self.config.inference.conf_threshold, verbose=False
            )
            elapsed = (time.time() - t0) * 1000

            annotated = results[0].plot()
            cv2.putText(annotated, f"{elapsed:.0f}ms", (10, 25),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

            out_path = out_dir / f"result_{img_path.name}"
            cv2.imwrite(str(out_path), annotated)

            if save_json:
                results_list.append(self._result_to_dict(results[0]))

            self._print_detections(results[0], prefix=f"[{i+1}/{len(images)}]")

        logger.info(f"批量推理完成，结果保存在: {out_dir}")

        if save_json and results_list:
            self._save_json(results_list, out_dir / "results.json")

        return True

    def export(self, format: str = 'onnx') -> bool:
        """导出模型"""
        if not self.model:
            if not self.load_model():
                return False

        logger.info(f"导出模型为 {format} 格式...")
        path = self.model.export(format=format)
        logger.info(f"导出完成: {path}")
        return True

    def _get_model_path(self) -> str:
        """获取模型路径"""
        if self.config.inference.model_path and Path(self.config.inference.model_path).exists():
            return self.config.inference.model_path
        latest = self.config.get_latest_model()
        if latest:
            return latest
        return ""

    def _result_to_dict(self, result) -> dict:
        """将YOLO结果转为字典"""
        detections = []
        if result.boxes is not None and len(result.boxes) > 0:
            boxes = result.boxes.xyxy.cpu().numpy()
            confs = result.boxes.conf.cpu().numpy()
            cls_ids = result.boxes.cls.cpu().numpy().astype(int)
            names = result.names

            for bbox, conf, cls_id in zip(boxes, confs, cls_ids):
                detections.append({
                    'class_id': int(cls_id),
                    'class_name': names.get(cls_id, str(cls_id)),
                    'confidence': float(conf),
                    'bbox': [float(v) for v in bbox]
                })

        return {
            'image_shape': list(result.orig_shape),
            'detections': detections
        }

    def _save_json(self, results_list, path=None):
        """保存JSON结果"""
        if path is None:
            path = "inference_results.json"

        with open(path, 'w', encoding='utf-8') as f:
            json.dump(results_list, f, indent=2, ensure_ascii=False)

        logger.info(f"JSON结果已保存: {path}")

    def _print_detections(self, result, prefix=""):
        """打印检测结果"""
        if result.boxes is None or len(result.boxes) == 0:
            logger.info(f"{prefix} 未检测到目标")
            return

        names = result.names
        logger.info(f"{prefix} 检测到 {len(result.boxes)} 个目标:")
        for i, (bbox, conf, cls_id) in enumerate(zip(
                result.boxes.xyxy.cpu().numpy(),
                result.boxes.conf.cpu().numpy(),
                result.boxes.cls.cpu().numpy().astype(int))):
            name = names.get(int(cls_id), str(cls_id))
            logger.info(f"  [{i}] {name} ({conf:.2f}) bbox=[{bbox[0]:.0f},{bbox[1]:.0f},{bbox[2]:.0f},{bbox[3]:.0f}]")


def parse_args():
    p = argparse.ArgumentParser(description='YOLOv8 推理部署脚本')
    p.add_argument('--config', default=None, help='配置文件路径')
    p.add_argument('--model', default=None, help='模型权重路径')
    p.add_argument('--source', default=None, help='输入源 (摄像头索引/图片/视频/目录)')
    p.add_argument('--conf', type=float, default=None, help='置信度阈值')
    p.add_argument('--save', action='store_true', help='保存结果')
    p.add_argument('--no-show', action='store_true', help='不显示窗口')
    p.add_argument('--save-json', action='store_true', help='保存JSON结果')
    p.add_argument('--export', choices=['onnx', 'engine', 'openvino', 'tflite', 'pb'],
                   help='导出模型格式')
    p.add_argument('--device', default=None, help='推理设备')
    return p.parse_args()


def main():
    args = parse_args()

    config = PipelineConfig(args.config)

    # 命令行参数覆盖
    if args.model:
        config.inference.model_path = args.model
    if args.source:
        config.inference.source = args.source
    if args.conf:
        config.inference.conf_threshold = args.conf
    if args.save:
        config.inference.save = True
    if args.no_show:
        config.inference.show = False
    if args.device:
        config.inference.device = 0 if args.device != 'cpu' else -1

    engine = InferenceEngine(config)

    if args.export:
        return 0 if engine.export(args.export) else 1

    return 0 if engine.run(save_json=args.save_json) else 1


if __name__ == "__main__":
    sys.exit(main())
