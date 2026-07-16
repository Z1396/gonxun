#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
推理部署脚本
功能：
1. 加载训练好的YOLOv8检测模型权重
2. 多输入源兼容：摄像头实时流、单张图片、视频文件、批量图片文件夹
3. 模型推理+目标框/类别可视化绘制
4. 可选保存推理输出：标注图片、标注视频、结构化JSON检测结果
5. 实时性能指标统计：单帧推理延迟、平均FPS
6. 支持模型导出：ONNX/TensorRT Engine/OpenVINO/TFLite/PB

使用方式:
  python inference.py                                    # 摄像头实时推理（默认0号摄像头）
  python inference.py --source image.jpg                 # 单张图片推理
  python inference.py --source video.mp4                 # 本地视频文件推理
  python inference.py --source ./test_images/            # 批量图片目录推理
  python inference.py --model runs/detect/exp/weights/best.pt  # 指定推理权重
  python inference.py --conf 0.3                         # 自定义检测置信度阈值
  python inference.py --save-json                        # 将所有检测框保存为json文件
  python inference.py --export onnx                      # 将pt权重导出onnx部署模型
"""
# 终端命令行参数解析
import argparse
# json序列化，用于导出结构化检测结果
import json
# 日志模块，分级打印运行信息、报错
import logging
# 系统模块，控制程序退出状态码
import sys
# 计时模块，统计推理耗时、FPS
import time
# 跨平台面向对象路径工具，替代os.path
from pathlib import Path

# OpenCV：图像读取、视频流、窗口显示、图像保存、绘制标注框
import cv2
# numpy：图像数组、推理结果数值计算、FPS均值统计
import numpy as np

# 外部自定义配置加载类，统一读取项目yaml配置
from config_loader import PipelineConfig

# 全局日志格式化配置：时间 + 日志等级 + 日志内容
logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s')
# 本脚本独立日志对象
logger = logging.getLogger(__name__)

# 捕获ultralytics导入异常，防止依赖缺失直接崩溃
try:
    from ultralytics import YOLO
    YOLO_AVAILABLE = True
except ImportError:
    YOLO_AVAILABLE = False
    logger.error("ultralytics未安装。请执行: pip install ultralytics")


class InferenceEngine:
    """YOLOv8 通用推理引擎
    完整功能链路：加载模型(含预热) → 自动识别输入源 → 分分支执行推理(摄像头/图片/视频/文件夹)
    → 可视化绘制 → 性能统计 → 保存图片/视频/json → 支持模型导出
    """

    def __init__(self, config: PipelineConfig):
        """构造函数
        :param config: PipelineConfig 全局配置对象，包含模型路径、置信度、输入源、保存/显示开关等
        """
        # 全局配置实例
        self.config = config
        # 推理模型实例，加载后赋值
        self.model = None
        # 存储最近30帧推理耗时，用于平滑计算平均FPS
        self.fps_counter = []

    def load_model(self) -> bool:
        """加载推理模型并执行模型预热
        :return: True 加载成功 / False 加载失败
        """
        # 前置校验：检测ultralytics库是否安装
        if not YOLO_AVAILABLE:
            logger.error("ultralytics未安装，无法加载模型")
            return False

        # 调用内部方法自动检索权重文件路径
        model_path = self._get_model_path()
        if not model_path:
            logger.error("未检索到合法模型权重文件，请检查路径或训练输出目录")
            return False

        logger.info(f"正在加载推理模型权重: {model_path}")
        # 实例化YOLO模型，加载pt权重
        self.model = YOLO(model_path)

        # 模型预热：传入空白640*640图片跑一次推理，消除首次推理耗时抖动
        dummy = np.zeros((640, 640, 3), dtype=np.uint8)
        self.model.predict(dummy, verbose=False)
        logger.info("模型预热完成，可开始推理")
        return True

    def run(self, save_json=False) -> bool:
        """推理总入口，自动识别输入源类型并分发对应推理逻辑
        :param save_json: bool 命令行开关，是否导出检测结果json
        :return: True 推理流程执行完成 / False 流程异常中断
        """
        # 模型未加载则先执行加载逻辑
        if not self.model:
            if not self.load_model():
                return False

        # 从配置读取输入源参数：摄像头ID/图片路径/视频路径/文件夹路径
        source = self.config.inference.source
        logger.info(f"当前推理输入源: {source}")

        # 分支1：输入为纯数字 → 摄像头设备ID
        if source.isdigit():
            return self._run_camera(int(source), save_json)
        # 分支2：输入是文件夹路径 → 批量图片推理
        elif Path(source).is_dir():
            return self._run_directory(source, save_json)
        # 分支3：输入是文件路径，区分视频/图片
        elif Path(source).is_file():
            ext = Path(source).suffix.lower()
            # 视频后缀列表
            if ext in {'.mp4', '.avi', '.mov', '.mkv', '.wmv'}:
                return self._run_video(source, save_json)
            # 其余视为普通图片
            else:
                return self._run_image(source, save_json)
        # 输入路径无效，无法识别
        else:
            logger.error(f"无法识别输入源格式: {source}")
            return False

    def _run_camera(self, camera_id: int, save_json: bool) -> bool:
        """摄像头实时流推理子流程
        :param camera_id: 摄像头设备编号，一般0为主摄像头
        :param save_json: 是否保存每一帧检测结果json
        :return: 执行状态布尔值
        """
        # 打开摄像头视频流
        cap = cv2.VideoCapture(camera_id)
        if not cap.isOpened():
            logger.error(f"摄像头 {camera_id} 打开失败，设备占用或不存在")
            return False

        logger.info("摄像头推理已启动，按键盘 q 退出窗口")
        # 存储所有帧检测结果，用于结束后统一写入json
        results_list = []

        try:
            # 循环读取摄像头每一帧画面
            while True:
                ret, frame = cap.read()
                # ret=False代表读取流结束/设备断开
                if not ret:
                    break

                # 记录单帧推理起始时间
                t0 = time.time()
                # 执行单帧推理，关闭冗余日志输出
                results = self.model.predict(
                    source=frame, conf=self.config.inference.conf_threshold,
                    verbose=False
                )
                # 计算当前帧推理耗时，单位毫秒
                elapsed = (time.time() - t0) * 1000

                # ultralytics内置方法绘制标注框、类别、置信度
                annotated = results[0].plot()

                # 更新耗时缓存队列，只保留最近30帧平滑FPS
                self.fps_counter.append(elapsed)
                if len(self.fps_counter) > 30:
                    self.fps_counter.pop(0)
                # 计算平均耗时与实时FPS
                avg_ms = np.mean(self.fps_counter)
                fps = 1000 / avg_ms if avg_ms > 0 else 0

                # 在画面左上角绘制FPS与单帧延迟文字
                cv2.putText(annotated, f"FPS: {fps:.1f} | {elapsed:.0f}ms",
                            (10, 25), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

                # 统计当前帧检测目标数量并绘制
                n_boxes = len(results[0].boxes) if results[0].boxes else 0
                cv2.putText(annotated, f"Detections: {n_boxes}",
                            (10, 50), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

                # 配置开启显示窗口则渲染画面
                if self.config.inference.show:
                    cv2.imshow('Inference', annotated)
                    # 监听按键，按下q跳出循环
                    if cv2.waitKey(1) & 0xFF == ord('q'):
                        break

                # 开启json保存则把当前帧检测结果存入列表
                if save_json:
                    results_list.append(self._result_to_dict(results[0]))

        except KeyboardInterrupt:
            # 捕获Ctrl+C手动中断
            logger.info("检测到用户手动中断推理")
        finally:
            # 释放摄像头硬件资源
            cap.release()
            # 销毁所有OpenCV窗口
            if self.config.inference.show:
                cv2.destroyAllWindows()

        # 存在检测结果且开启json保存，写入本地文件
        if save_json and results_list:
            self._save_json(results_list)

        return True

    def _run_image(self, img_path: str, save_json: bool) -> bool:
        """单张图片推理子流程
        :param img_path: 图片文件路径
        :param save_json: 是否保存该图检测结果json
        """
        img = cv2.imread(img_path)
        if img is None:
            logger.error(f"图片读取失败，路径：{img_path}")
            return False

        t0 = time.time()
        # 图片推理
        results = self.model.predict(
            source=img, conf=self.config.inference.conf_threshold, verbose=False
        )
        elapsed = (time.time() - t0) * 1000

        # 绘制标注结果
        annotated = results[0].plot()
        # 画面左上角标注推理耗时
        cv2.putText(annotated, f"{elapsed:.0f}ms", (10, 25),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

        # 配置开启保存图片，输出带标注的图片
        if self.config.inference.save:
            out_path = f"inference_result_{Path(img_path).name}"
            cv2.imwrite(out_path, annotated)
            logger.info(f"标注图片已保存至: {out_path}")

        # 开启窗口显示，等待任意按键关闭
        if self.config.inference.show:
            cv2.imshow('Inference', annotated)
            cv2.waitKey(0)
            cv2.destroyAllWindows()

        # 导出json检测结果
        if save_json:
            self._save_json([self._result_to_dict(results[0])])

        # 控制台打印该图所有目标框详情
        self._print_detections(results[0])

        return True

    def _run_video(self, video_path: str, save_json: bool) -> bool:
        """本地视频文件推理子流程，自动输出标注后的完整视频
        :param video_path: 视频文件路径
        :param save_json: 保存每一帧检测结果
        """
        cap = cv2.VideoCapture(video_path)
        if not cap.isOpened():
            logger.error(f"视频文件打开失败: {video_path}")
            return False

        # 获取视频原始宽、高、帧率
        w = int(cap.get(cv2.CAP_PROP_FRAME_WIDTH))
        h = int(cap.get(cv2.CAP_PROP_FRAME_HEIGHT))
        fps = cap.get(cv2.CAP_PROP_FPS)

        # 输出视频文件名，mp4编码
        out_path = f"inference_result_{Path(video_path).name}"
        fourcc = cv2.VideoWriter_fourcc(*'mp4v')
        writer = cv2.VideoWriter(out_path, fourcc, fps, (w, h))

        results_list = []
        frame_count = 0

        logger.info(f"视频参数：宽{w} 高{h} 帧率{fps:.1f}")
        try:
            while True:
                ret, frame = cap.read()
                if not ret:
                    break

                # 单帧推理并绘制标注
                results = self.model.predict(
                    source=frame, conf=self.config.inference.conf_threshold,
                    verbose=False
                )
                annotated = results[0].plot()
                # 写入输出视频流
                writer.write(annotated)

                # 保存帧检测数据
                if save_json:
                    results_list.append(self._result_to_dict(results[0]))

                frame_count += 1
                # 每30帧打印一次进度日志
                if frame_count % 30 == 0:
                    logger.info(f"已处理视频帧数：{frame_count}")

                # 实时窗口显示
                if self.config.inference.show:
                    cv2.imshow('Inference', annotated)
                    if cv2.waitKey(1) & 0xFF == ord('q'):
                        break
        except KeyboardInterrupt:
            pass
        finally:
            # 释放视频读写资源
            cap.release()
            writer.release()
            if self.config.inference.show:
                cv2.destroyAllWindows()

        logger.info(f"标注视频已保存：{out_path}，总处理帧数 {frame_count}")

        # 写入视频全帧检测json
        if save_json and results_list:
            self._save_json(results_list)

        return True

    def _run_directory(self, dir_path: str, save_json: bool) -> bool:
        """图片文件夹批量推理子流程
        :param dir_path: 图片文件夹路径
        :param save_json: 批量所有图片检测结果合并存入一个json
        """
        img_dir = Path(dir_path)
        # 支持的图片后缀集合
        img_exts = {'.jpg', '.jpeg', '.png', '.bmp', '.webp'}
        # 过滤目录内所有图片文件
        images = [f for f in img_dir.iterdir() if f.suffix.lower() in img_exts]

        if not images:
            logger.error(f"目标目录内未找到任何图片文件: {dir_path}")
            return False

        logger.info(f"批量推理，共找到 {len(images)} 张图片")
        # 在原图目录新建results文件夹存放输出图
        out_dir = img_dir / "results"
        out_dir.mkdir(exist_ok=True)

        results_list = []
        # 遍历所有图片执行推理
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
            # 标注推理耗时
            cv2.putText(annotated, f"{elapsed:.0f}ms", (10, 25),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)

            # 保存标注图片至results子文件夹
            out_path = out_dir / f"result_{img_path.name}"
            cv2.imwrite(str(out_path), annotated)

            # 收集检测结果
            if save_json:
                results_list.append(self._result_to_dict(results[0]))

            # 打印单张图片检测信息，附带批量进度前缀
            self._print_detections(results[0], prefix=f"[{i+1}/{len(images)}]")

        logger.info(f"批量推理全部完成，标注图片输出目录: {out_dir}")

        # 批量图片检测结果统一存入results.json
        if save_json and results_list:
            self._save_json(results_list, out_dir / "results.json")

        return True

    def export(self, format: str = 'onnx') -> bool:
        """模型导出功能，将pt权重转为部署格式
        :param format: 导出格式支持 onnx / engine / openvino / tflite / pb
        :return: 导出成功布尔值
        """
        # 未加载模型先加载
        if not self.model:
            if not self.load_model():
                return False

        logger.info(f"开始导出模型至 {format} 部署格式...")
        # ultralytics内置export接口自动完成转换
        path = self.model.export(format=format)
        logger.info(f"模型导出完成，输出路径: {path}")
        return True

    def _get_model_path(self) -> str:
        """私有工具：多优先级自动查找推理权重
        优先级：命令行/配置指定路径 > 最新训练生成权重 > 空字符串
        """
        # 优先级1：配置文件手动指定模型路径
        if self.config.inference.model_path and Path(self.config.inference.model_path).exists():
            return self.config.inference.model_path
        # 优先级2：调用配置内置方法获取最新训练exp的best.pt
        latest = self.config.get_latest_model()
        if latest:
            return latest
        # 无可用模型返回空
        return ""

    def _result_to_dict(self, result) -> dict:
        """私有工具：将YOLO推理Result对象转换为可序列化字典，用于保存JSON
        :param result: ultralytics.yolo.engine.results.Result 单帧推理结果
        :return: 结构化字典，包含原图尺寸、所有目标框信息
        """
        detections = []
        # 判断存在检测框
        if result.boxes is not None and len(result.boxes) > 0:
            # 提取框坐标xyxy、置信度、类别ID，转CPU numpy数组
            boxes = result.boxes.xyxy.cpu().numpy()
            confs = result.boxes.conf.cpu().numpy()
            cls_ids = result.boxes.cls.cpu().numpy().astype(int)
            # 类别名称映射字典 {id: "类别名"}
            names = result.names

            # 遍历每个目标框组装字典
            for bbox, conf, cls_id in zip(boxes, confs, cls_ids):
                detections.append({
                    'class_id': int(cls_id),
                    'class_name': names.get(cls_id, str(cls_id)),
                    'confidence': float(conf),
                    'bbox': [float(v) for v in bbox]
                })

        return {
            'image_shape': list(result.orig_shape),  # 原图高宽通道
            'detections': detections                 # 所有目标列表
        }

    def _save_json(self, results_list, path=None):
        """私有工具：将推理结果列表写入json文件
        :param results_list: 多帧/多图的检测字典列表
        :param path: json输出路径，不传则默认 inference_results.json
        """
        if path is None:
            path = "inference_results.json"

        with open(path, 'w', encoding='utf-8') as f:
            # indent=2格式化输出，ensure_ascii=False支持中文类别名
            json.dump(results_list, f, indent=2, ensure_ascii=False)

        logger.info(f"结构化检测JSON结果已保存至: {path}")

    def _print_detections(self, result, prefix=""):
        """私有工具：控制台格式化打印单张图/单帧所有检测目标
        :param result: 单帧推理结果对象
        :param prefix: 自定义前缀，批量推理时显示进度
        """
        # 无检测目标直接打印提示
        if result.boxes is None or len(result.boxes) == 0:
            logger.info(f"{prefix} 未检测到任何目标")
            return

        names = result.names
        logger.info(f"{prefix} 检测到 {len(result.boxes)} 个目标:")
        # 遍历所有框打印类别、置信度、像素坐标
        for i, (bbox, conf, cls_id) in enumerate(zip(
                result.boxes.xyxy.cpu().numpy(),
                result.boxes.conf.cpu().numpy(),
                result.boxes.cls.cpu().numpy().astype(int))):
            name = names.get(int(cls_id), str(cls_id))
            logger.info(f"  [{i}] {name} ({conf:.2f}) bbox=[{bbox[0]:.0f},{bbox[1]:.0f},{bbox[2]:.0f},{bbox[3]:.0f}]")


def parse_args():
    """解析终端命令行启动参数，支持覆盖配置文件参数、导出模型等开关
    :return: args 参数对象
    """
    p = argparse.ArgumentParser(description='YOLOv8 通用推理部署脚本，支持图片/视频/摄像头/批量推理与模型导出')
    p.add_argument('--config', default=None, help='项目yaml配置文件路径')
    p.add_argument('--model', default=None, help='推理使用的pt模型权重路径')
    p.add_argument('--source', default=None, help='推理输入源：摄像头数字/图片/视频/文件夹路径')
    p.add_argument('--conf', type=float, default=None, help='检测置信度过滤阈值(0~1)')
    p.add_argument('--save', action='store_true', help='开启保存带标注的图片/视频')
    p.add_argument('--no-show', action='store_true', help='关闭实时可视化窗口')
    p.add_argument('--save-json', action='store_true', help='导出所有检测框结构化JSON文件')
    p.add_argument('--export', choices=['onnx', 'engine', 'openvino', 'tflite', 'pb'],
                   help='导出模型部署格式，指定后仅执行导出不推理')
    p.add_argument('--device', default=None, help='推理硬件设备：显卡编号 / cpu')
    return p.parse_args()


def main():
    """程序主入口函数
    流程：解析参数 → 加载配置 → 命令行参数覆盖配置 → 初始化推理引擎
    → 判断是否执行模型导出 / 执行推理流程 → 返回程序退出码
    """
    args = parse_args()

    # 加载配置文件
    config = PipelineConfig(args.config)

    # 命令行参数优先级高于yaml，覆盖对应配置字段
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
        # cpu标记为-1，其余参数视为GPU卡号
        config.inference.device = 0 if args.device != 'cpu' else -1

    # 实例化推理引擎
    engine = InferenceEngine(config)

    # 若传入--export，仅执行模型导出，不执行推理
    if args.export:
        return 0 if engine.export(args.export) else 1

    # 执行完整推理流程，返回退出码
    return 0 if engine.run(save_json=args.save_json) else 1


# 脚本直接运行时才执行main，作为模块导入不自动运行
if __name__ == "__main__":
    sys.exit(main())