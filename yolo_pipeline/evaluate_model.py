#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
模型评估脚本
功能：
1. 加载训练好的YOLOv8模型权重
2. 在划分好的验证集上运行评估，计算全套检测指标
3. 自动生成混淆矩阵、PR曲线、F1曲线等可视化图表
4. 读取配置里的指标阈值，自动对比并输出是否达标结论
5. 输出每个类别的独立AP指标，完成细粒度数据分析

使用方式:
  python evaluate_model.py                              # 使用默认yaml配置文件
  python evaluate_model.py --config config.yaml         # 指定自定义配置文件
  python evaluate_model.py --model runs/detect/exp/weights/best.pt  # 手动指定评估权重
  python evaluate_model.py --conf 0.3                   # 覆盖置信度过滤阈值
  python evaluate_model.py --plot                       # 开启绘图（代码内默认已开启）
"""
# 命令行参数解析库，解析终端输入的启动参数
import argparse
# json模块，用于将评估指标结构化保存为报告文件
import json
# 日志模块，分级打印运行日志、提示、错误信息
import logging
# 系统标准库，控制程序退出返回码
import sys
# Path面向对象路径工具，替代os.path，跨平台路径拼接更简洁安全
from pathlib import Path

# 外部自定义配置加载类，统一读取项目yaml配置
from config_loader import PipelineConfig

# 全局日志格式化配置：打印时间 + 日志等级 + 日志内容
logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s')
# 创建本脚本独立日志对象
logger = logging.getLogger(__name__)

# 捕获ultralytics导入异常，避免未装库直接崩溃
try:
    # YOLOv8官方库核心模型类
    from ultralytics import YOLO
    # 标记库可用状态
    YOLO_AVAILABLE = True
except ImportError:
    YOLO_AVAILABLE = False
    logger.error("ultralytics依赖未安装。请执行命令：pip install ultralytics")


class ModelEvaluator:
    """YOLOv8模型评估器核心类
    完整评估流水线：查找模型权重 -> 加载模型 -> 验证集评估 -> 打印指标报告 -> 保存json结果 -> 校验指标是否达标
    """

    def __init__(self, config: PipelineConfig):
        """构造函数
        :param config: PipelineConfig 全局配置实例，包含数据集路径、评估超参、类别列表、指标阈值等
        """
        # 保存全局配置到实例变量，所有内部方法均可读取
        self.config = config

    def evaluate(self) -> bool:
        """评估流程主入口函数
        :return: bool True=模型达到预设指标，False=未达标/运行异常
        """
        # 前置校验：检测YOLO库是否正常导入
        if not YOLO_AVAILABLE:
            logger.error("无法执行评估：ultralytics库缺失，请先安装依赖")
            return False

        # 步骤1：自动检索可用模型权重路径
        model_path = self._get_model_path()
        if not model_path:
            logger.error("未检索到合法模型权重文件，请检查路径或训练输出目录")
            return False

        logger.info(f"成功读取待评估模型：{model_path}")
        # 实例化YOLO模型，加载权重
        model = YOLO(model_path)

        # 步骤2：调用val()在验证集执行评估
        logger.info("开始在验证集运行模型评估，计算各类检测指标...")
        results = model.val(
            # 数据集yaml配置文件路径
            data=self.config.get_data_yaml_path(),
            # 检测框置信度过滤阈值
            conf=self.config.evaluation.conf_threshold,
            # NMS非极大抑制IoU阈值
            iou=self.config.evaluation.iou_threshold,
            # 推理设备：GPU卡号 / cpu
            device=self.config.inference.device if self.config.inference.device >= 0 else 'cpu',
            plots=True,        # 开启自动绘图：混淆矩阵、PR曲线、F1曲线、预测图
            save_json=True,    # 内置保存评估原始json结果
        )

        # 步骤3：控制台打印格式化完整评估报告
        self._print_report(results)

        # 步骤4：结构化指标保存为本地json报告文件
        self._save_results(results)

        # 步骤5：对比配置中的目标mAP阈值，判断模型是否合格
        passed = self._check_targets(results)

        return passed

    def _get_model_path(self) -> str:
        """私有工具函数：多优先级自动查找待评估模型权重
        优先级：命令行/配置指定路径 > 最新训练输出权重 > 默认best.pt路径
        :return: str 模型文件绝对路径，无模型返回空字符串
        """
        # 优先级1：配置文件/命令行手动指定的模型路径
        if self.config.inference.model_path and Path(self.config.inference.model_path).exists():
            return self.config.inference.model_path

        # 优先级2：调用配置内置方法获取最新一次训练生成的权重
        latest = self.config.get_latest_model()
        if latest:
            return latest

        # 优先级3：兜底默认路径：当前项目输出目录下exp权重best.pt
        default = Path(self.config.project.output_dir) / self.config.project.name / "weights" / "best.pt"
        if default.exists():
            return str(default)

        # 全部路径均无匹配文件，返回空
        return ""

    def _print_report(self, results):
        """私有工具函数：控制台格式化打印评估指标报告
        分为三部分：全局整体指标、逐类别AP指标、与目标阈值对比结果
        :param results: YOLO.val()返回的完整评估结果对象
        """
        # 打印分割线区分报告区域
        logger.info("\n" + "=" * 60)
        logger.info("============= 模型完整评估报告 =============")
        logger.info("=" * 60)

        # ---------------------- 全局整体指标 ----------------------
        logger.info(f"\n【全局综合指标】")
        logger.info(f"  mAP50:    {results.box.map50:.4f}")    # IoU=0.5下平均精度
        logger.info(f"  mAP50-95: {results.box.map:.4f}")      # IoU 0.5~0.95区间平均精度，官方核心指标
        logger.info(f"  Precision:{results.box.mp:.4f}")      # 全局平均精确率
        logger.info(f"  Recall:   {results.box.mr:.4f}")      # 全局平均召回率

        # ---------------------- 逐类别独立指标 ----------------------
        logger.info(f"\n【逐类别细分指标】")
        # 格式化表头，对齐输出
        logger.info(f"  {'类别':<20} {'mAP50':>8} {'mAP50-95':>10} {'Precision':>10} {'Recall':>8}")
        logger.info(f"  {'-'*20} {'-'*8} {'-'*10} {'-'*10} {'-'*8}")

        # 遍历配置内所有类别名称，匹配对应AP数值
        for i, name in enumerate(self.config.class_names):
            # 防止类别数量与评估结果数量不匹配数组越界
            if i < len(results.box.ap50):
                ap50 = results.box.ap50[i]
                ap = results.box.ap[i]
                # 单类别打印，精度/召回无单类简化输出填占位符'-'
                logger.info(f"  {name:<20} {ap50:>8.4f} {ap:>10.4f} {'-':>10} {'-':>8}")

        # ---------------------- 指标达标对比 ----------------------
        ev = self.config.evaluation
        logger.info(f"\n【指标目标阈值对比】")
        # 判断两项核心指标是否达标
        map50_ok = results.box.map50 >= ev.target_map50
        map_ok = results.box.map >= ev.target_map
        logger.info(f"  mAP50目标阈值: {ev.target_map50} {'✅ 达标' if map50_ok else '❌ 未达标'}")
        logger.info(f"  mAP50-95目标阈值:   {ev.target_map} {'✅ 达标' if map_ok else '❌ 未达标'}")

    def _save_results(self, results):
        """私有工具函数：将评估指标结构化写入evaluation_report.json
        保存内容：模型路径、全局指标、每类指标、预设目标阈值，方便后续复盘/自动化读取
        :param results: YOLO.val()评估结果对象
        """
        # 报告输出目录：本次实验输出文件夹
        output_dir = Path(self.config.project.output_dir) / self.config.project.name
        # 不存在则递归创建目录
        output_dir.mkdir(parents=True, exist_ok=True)

        # 构建待保存字典结构
        report = {
            # 评估使用的权重文件路径
            'model': self._get_model_path(),
            # 全局整体指标
            'metrics': {
                'mAP50': float(results.box.map50),
                'mAP50-95': float(results.box.map),
                'precision': float(results.box.mp),
                'recall': float(results.box.mr),
            },
            # 空字典，后续填充每个类别的AP
            'per_class': {},
            # 配置文件中设定的合格指标阈值
            'targets': {
                'target_map50': self.config.evaluation.target_map50,
                'target_map': self.config.evaluation.target_map,
            }
        }

        # 循环填充每一类指标
        for i, name in enumerate(self.config.class_names):
            if i < len(results.box.ap50):
                report['per_class'][name] = {
                    'mAP50': float(results.box.ap50[i]),
                    'mAP50-95': float(results.box.ap[i]),
                }

        # 拼接报告完整路径
        report_path = output_dir / "evaluation_report.json"
        # 写入json文件，indent=2格式化换行，ensure_ascii=False支持中文类别名
        with open(report_path, 'w', encoding='utf-8') as f:
            json.dump(report, f, indent=2, ensure_ascii=False)

        logger.info(f"\n结构化评估报告已保存至：{report_path}")

    def _check_targets(self, results) -> bool:
        """私有工具函数：校验模型是否同时满足两项核心mAP阈值，输出优化建议
        :param results: YOLO评估结果对象
        :return: bool 两项指标全部达标返回True，否则False
        """
        ev = self.config.evaluation
        # 分别判断两项核心指标是否超过目标阈值
        map50_ok = results.box.map50 >= ev.target_map50
        map_ok = results.box.map >= ev.target_map

        # 双指标全部达标
        if map50_ok and map_ok:
            logger.info("\n🎉 模型全部指标达到预设标准，可投入部署使用！")
        else:
            logger.info("\n⚠️  模型未达到全部目标指标，优化建议参考：")
            # 针对mAP50不足给出优化方向
            if not map50_ok:
                logger.info("  1. mAP50偏低优化方案：")
                logger.info("     - 增加训练轮数，延长拟合时间")
                logger.info("     - 扩充数据集或增强数据增强强度")
            # 针对mAP50-95不足给出优化方向
            if not map_ok:
                logger.info("  2. mAP50-95偏低优化方案：")
                logger.info("     - 调整初始学习率、批次大小、优化器参数")
                logger.info("     - 人工检查标注是否漏标、框位置不准确")

        # 返回双指标同时合格的布尔结果
        return map50_ok and map_ok


def parse_args():
    """解析终端命令行传入参数，支持覆盖配置文件内参数
    :return: args 对象，存储所有传入的命令行参数
    """
    # 创建参数解析器，添加脚本描述
    p = argparse.ArgumentParser(description='YOLOv8 目标检测模型自动化评估脚本')
    # 指定外部yaml配置文件路径
    p.add_argument('--config', default=None, help='项目配置yaml文件路径')
    # 手动指定评估权重，优先级高于配置文件
    p.add_argument('--model', default=None, help='待评估模型权重pt文件路径')
    # 覆盖评估置信度阈值
    p.add_argument('--conf', type=float, default=None, help='检测置信度过滤阈值(0~1)')
    # 覆盖NMS IoU阈值
    p.add_argument('--iou', type=float, default=None, help='NMS非极大抑制IoU阈值(0~1)')
    # 临时覆盖mAP50合格标准
    p.add_argument('--target-map50', type=float, default=None, help='mAP50达标最低阈值')
    # 临时覆盖mAP50-95合格标准
    p.add_argument('--target-map', type=float, default=None, help='mAP50-95达标最低阈值')
    return p.parse_args()


def main():
    """程序主入口函数
    执行逻辑：解析参数 -> 加载配置 -> 命令行参数覆盖配置 -> 初始化评估器 -> 运行评估 -> 返回退出码
    :return: int 0=模型达标正常退出，1=未达标/运行失败
    """
    # 获取终端传入参数
    args = parse_args()

    # 加载配置文件，无--config则读取默认配置
    config = PipelineConfig(args.config)

    # 命令行参数优先级高于yaml配置，逐个覆盖对应配置字段
    if args.model:
        config.inference.model_path = args.model
    if args.conf:
        config.evaluation.conf_threshold = args.conf
    if args.iou:
        config.evaluation.iou_threshold = args.iou
    if args.target_map50:
        config.evaluation.target_map50 = args.target_map50
    if args.target_map:
        config.evaluation.target_map = args.target_map

    # 实例化评估器
    evaluator = ModelEvaluator(config)
    # 执行完整评估流程，获取是否达标标记
    passed = evaluator.evaluate()

    # 达标返回0，未达标返回1，给上层脚本/流水线识别状态
    return 0 if passed else 1


# 程序入口判断：只有直接运行该脚本时才执行main，作为模块导入时不自动执行
if __name__ == "__main__":
    # 接收main返回的状态码退出程序
    sys.exit(main())