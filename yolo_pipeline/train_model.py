#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
模型训练脚本
功能：
1. 加载预训练 YOLOv8 模型
2. 根据配置文件设置训练参数
3. 执行训练并保存权重
4. 输出训练曲线和指标

使用方式:
  python train_model.py                              # 使用默认配置
  python train_model.py --config config.yaml         # 指定配置文件
  python train_model.py --epochs 200                 # 覆盖训练轮数
  python train_model.py --model yolov8s.pt           # 使用更大模型
  python train_model.py --batch 32                   # 调整批次大小
  python train_model.py --device cpu                 # 使用CPU训练
"""
# 解析终端命令行输入参数
import argparse
# 日志打印模块，分级输出运行信息、警告、错误
import logging
# 系统模块，用于程序退出时返回状态码
import sys
# 面向对象路径工具，跨平台兼容，替代老旧os.path
from pathlib import Path

# 外部自定义配置加载类，统一读取yaml项目配置
from config_loader import PipelineConfig

# 全局日志基础配置：打印时间、日志等级、日志内容
logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s')
# 创建当前脚本独立日志实例
logger = logging.getLogger(__name__)

# 捕获ultralytics导入异常，防止未安装依赖直接程序崩溃
try:
    # YOLOv8官方核心模型类
    from ultralytics import YOLO
    # 标记YOLO库可用
    YOLO_AVAILABLE = True
except ImportError:
    YOLO_AVAILABLE = False
    logger.error("ultralytics未安装。请执行: pip install ultralytics")


class ModelTrainer:
    """YOLOv8 模型训练器
    封装完整训练流水线：校验数据集 -> 加载预训练权重 -> 组装训练超参 -> 启动训练 -> 打印训练结果汇总
    """

    def __init__(self, config: PipelineConfig):
        """构造函数
        :param config: PipelineConfig 全局配置对象，包含数据集、训练超参、输出路径、类别等全部配置信息
        """
        # 将全局配置保存为实例属性，所有内部方法均可读取使用
        self.config = config

    def train(self) -> bool:
        """训练流程主入口函数
        :return: bool True=训练流程正常执行完成；False=前置校验失败，无法启动训练
        """
        # 前置校验：判断YOLO依赖库是否正常导入
        if not YOLO_AVAILABLE:
            logger.error("无法训练：ultralytics未安装")
            return False

        # 步骤1：校验数据集配置文件data.yaml是否存在
        data_yaml = self.config.get_data_yaml_path()
        if not Path(data_yaml).exists():
            logger.error(f"data.yaml 不存在，请先运行 prepare_data.py 生成数据集与配置文件")
            return False

        # 步骤2：加载预训练YOLOv8模型权重（yolov8n/s/m/l/x.pt）
        logger.info(f"加载预训练模型权重文件: {self.config.training.model}")
        model = YOLO(self.config.training.model)

        # 步骤3：调用内部方法，从配置提取所有参数，组装train函数所需字典
        train_args = self._build_train_args()

        # 步骤4：启动YOLO训练，将组装好的参数解包传入
        logger.info(f"开始训练，本次训练参数详情: {train_args}")
        results = model.train(**train_args)

        # 步骤5：训练结束，打印权重路径、文件大小、各项指标、日志路径
        self._print_results(results)

        # 全流程无异常，返回True
        return True

    def _build_train_args(self) -> dict:
        """私有工具方法：读取配置类中的训练参数，构建model.train()所需完整参数字典
        :return: dict 可直接解包传入YOLO训练函数的参数字典
        """
        # 简写训练配置分组，减少重复代码
        t = self.config.training
        return {
            # 数据集yaml配置文件路径
            'data': self.config.get_data_yaml_path(),
            # 总训练轮数 epoch
            'epochs': t.epochs,
            # 模型输入图像统一缩放尺寸
            'imgsz': t.imgsz,
            # 每批次训练图片数量
            'batch': t.batch_size,
            # 训练设备：GPU卡号数字 / cpu字符串
            'device': t.device if t.device >= 0 else 'cpu',
            # DataLoader加载数据的子进程数量
            'workers': t.workers,

            # 学习率相关参数
            'lr0': t.lr0,       # 初始学习率
            'lrf': t.lrf,       # 最终学习率衰减系数，lr_final = lr0 * lrf

            # 早停策略：连续patience轮mAP无提升则提前终止训练
            'patience': t.patience,

            # 模型保存相关配置
            'save_period': t.save_period,  # 每隔多少轮保存一次中间权重
            'project': self.config.project.output_dir,  # 训练输出根目录
            'name': self.config.project.name,            # 本次实验文件夹名称

            # 优化器自动选择（AdamW/SGD由库自动适配）
            'optimizer': 'auto',

            # 日志与可视化
            'verbose': True,  # 每轮打印详细训练日志
            'plots': True,    # 自动生成loss曲线、mAP曲线等图表
        }

    def _print_results(self, results):
        """私有工具方法：训练完成后格式化打印训练汇总信息
        :param results: model.train()返回的训练结果对象，包含全部指标数据
        """
        # 分割线区分训练结果板块
        logger.info("\n" + "=" * 60)
        logger.info("训练完成!")
        logger.info("=" * 60)

        # 拼接最佳权重best.pt完整路径
        best_pt = Path(self.config.project.output_dir) / self.config.project.name / "weights" / "best.pt"
        if best_pt.exists():
            logger.info(f"最优模型权重路径: {best_pt}")
            # 转换文件大小单位为MB并保留两位小数
            logger.info(f"权重文件大小: {best_pt.stat().st_size / 1024 / 1024:.2f} MB")

        # 打印最终各项训练指标（loss、precision、recall、mAP等）
        if hasattr(results, 'results_dict'):
            metrics = results.results_dict
            logger.info(f"\n最终训练指标:")
            for key, val in metrics.items():
                # 只打印数字类型指标，过滤无关文本字段
                if isinstance(val, (int, float)):
                    logger.info(f"  {key}: {val:.4f}")

        # 打印日志保存目录与TensorBoard查看指令
        log_dir = Path(self.config.project.output_dir) / self.config.project.name
        logger.info(f"\n训练日志、图表保存目录: {log_dir}")
        logger.info("使用 TensorBoard 实时查看训练曲线，执行命令: tensorboard --logdir " + str(log_dir))


def parse_args():
    """解析终端传入的命令行参数，支持覆盖配置文件内的训练参数
    :return: args 对象，存储所有命令行输入参数
    """
    # 创建参数解析器，添加脚本功能描述
    p = argparse.ArgumentParser(description='YOLOv8 目标检测模型自动化训练脚本')
    # 指定外部yaml配置文件路径
    p.add_argument('--config', default=None, help='配置文件路径')
    # 手动指定预训练模型权重
    p.add_argument('--model', default=None, help='预训练模型 (yolov8n/s/m/l/x.pt)')
    # 覆盖训练总轮数
    p.add_argument('--epochs', type=int, default=None, help='训练轮数')
    # 覆盖批次大小
    p.add_argument('--batch', type=int, default=None, help='批次大小')
    # 覆盖输入图像尺寸
    p.add_argument('--imgsz', type=int, default=None, help='图像尺寸')
    # 覆盖训练设备（显卡编号或cpu）
    p.add_argument('--device', default=None, help='推理设备 (0/cpu)')
    # 覆盖初始学习率
    p.add_argument('--lr0', type=float, default=None, help='初始学习率')
    # 覆盖早停耐心值
    p.add_argument('--patience', type=int, default=None, help='早停耐心值')
    # 覆盖本次实验文件夹名称
    p.add_argument('--name', default=None, help='实验名称')
    return p.parse_args()


def main():
    """程序主入口函数
    执行流程：解析命令行参数 -> 加载配置文件 -> 命令行参数覆盖配置 -> 实例化训练器 -> 执行训练 -> 返回程序退出码
    :return: int 0=训练正常完成；1=训练失败/校验不通过
    """
    # 获取终端输入的所有参数
    args = parse_args()

    # 加载配置文件，无--config参数则读取项目默认配置
    config = PipelineConfig(args.config)

    # 命令行参数优先级高于yaml配置，逐个覆盖对应配置字段
    if args.model:
        config.training.model = args.model
    if args.epochs:
        config.training.epochs = args.epochs
    if args.batch:
        config.training.batch_size = args.batch
    if args.imgsz:
        config.training.imgsz = args.imgsz
    if args.device:
        # 传入cpu则标记设备为-1，其余为显卡卡号
        config.training.device = 0 if args.device != 'cpu' else -1
    if args.lr0:
        config.training.lr0 = args.lr0
    if args.patience:
        config.training.patience = args.patience
    if args.name:
        config.project.name = args.name

    # 实例化训练器对象
    trainer = ModelTrainer(config)
    # 启动完整训练流程
    success = trainer.train()

    # 训练成功返回0，失败返回1，供流水线脚本识别运行状态
    return 0 if success else 1


# 程序入口判断：仅直接运行本脚本时执行main；作为模块被导入时不自动运行训练
if __name__ == "__main__":
    # 接收main函数返回的状态码，安全退出程序
    sys.exit(main())