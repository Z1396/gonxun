#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
一键运行流水线总控脚本
功能：按顺序串联整套YOLO目标检测全流程，支持全流程一键执行或分步单独执行
完整流程顺序：数据准备 prepare → 模型训练 train → 模型评估 evaluate → 推理部署 inference
1. 支持单步执行、多步骤组合执行、完整流水线自动串行执行
2. 统一接收命令行参数，全局覆盖配置文件参数，统一向下游4个子脚本透传
3. 记录每一步运行耗时、执行状态，流程中断自动停止并打印报错
4. 全部步骤完成后输出完整执行汇总报告，包含总耗时、模型路径、数据集路径

使用方式:
  python run_pipeline.py                     # 完整流水线：数据准备→训练→评估→推理
  python run_pipeline.py --step prepare      # 仅执行数据准备步骤
  python run_pipeline.py --step train        # 仅执行模型训练步骤
  python run_pipeline.py --step evaluate     # 仅执行模型评估步骤
  python run_pipeline.py --step inference    # 仅执行推理部署步骤
  python run_pipeline.py --step prepare,train # 组合执行：数据准备 + 模型训练
  python run_pipeline.py --epochs 200        # 全局覆盖训练轮数参数
  python run_pipeline.py --raw_dir ./images  # 全局指定原始图片目录
"""
# 解析终端命令行启动参数
import argparse
# 日志模块，统一管控全流水线日志输出
import logging
# 系统标准库：路径导入、程序退出状态码返回
import sys
# 计时模块：统计单步骤耗时、流水线总耗时
import time
# 面向对象路径处理，跨平台兼容文件/目录路径
from pathlib import Path

# 将当前脚本所在目录加入Python模块搜索路径，确保同目录下4个子脚本可正常import
sys.path.insert(0, str(Path(__file__).parent))

# 全局统一配置加载类，所有子脚本共用同一套配置对象
from config_loader import PipelineConfig

# 全局日志格式化配置：打印时间 + 日志等级 + 日志内容
logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s')
# 创建本脚本专属日志实例
logger = logging.getLogger(__name__)

# 定义流水线全部合法步骤，固定执行顺序
STEPS = ['prepare', 'train', 'evaluate', 'inference']


class PipelineRunner:
    """YOLO检测流水线总调度器
    核心职责：
    1. 接收全局配置与待执行步骤列表
    2. 按顺序循环执行每个步骤，捕获单步异常
    3. 记录每一步成功/失败状态、运行耗时
    4. 单步失败直接中断整条流水线
    5. 全部执行完成后输出汇总统计报告
    """

    def __init__(self, config: PipelineConfig, steps: list):
        """构造函数
        :param config: PipelineConfig 全局统一配置实例，向下游所有子脚本透传
        :param steps: list[str] 用户指定的待执行步骤列表，按传入顺序执行
        """
        # 全局配置对象
        self.config = config
        # 需要执行的步骤列表
        self.steps = steps
        # 存储每一步执行结果：key=步骤名，value={success:布尔, elapsed:耗时秒数}
        self.results = {}

    def run(self) -> bool:
        """流水线总执行入口主函数
        :return: bool True=所有步骤全部执行成功；False=任意步骤执行失败，流水线中断
        """
        # 打印流水线头部分隔标题
        logger.info("=" * 60)
        logger.info("YOLO 目标检测完整自动化流水线")
        logger.info("=" * 60)
        logger.info(f"当前全局配置参数: {self.config}")
        logger.info(f"本次待执行步骤列表: {self.steps}")
        logger.info("")

        # 记录流水线整体起始时间，用于计算总耗时
        total_start = time.time()

        # 循环依次执行每一个步骤
        for step in self.steps:
            # 记录单步骤起始时间
            step_start = time.time()
            logger.info(f"\n{'='*40}")
            logger.info(f"【开始执行步骤】 {step.upper()}")
            logger.info(f"{'='*40}")

            # 调用内部方法执行当前步骤，获取执行成功标记
            success = self._run_step(step)
            # 计算当前步骤总耗时
            elapsed = time.time() - step_start

            # 将步骤执行结果存入结果字典
            self.results[step] = {
                'success': success,
                'elapsed': elapsed
            }

            # 根据执行状态打印日志
            if success:
                logger.info(f"✅ {step} 步骤执行完成，耗时({elapsed:.1f}s)")
            else:
                logger.error(f"❌ {step} 步骤执行失败，耗时({elapsed:.1f}s)")
                logger.warning("流水线终止，不再执行后续步骤")
                # 单步失败直接返回False，中断整条流水线
                return False

        # 全部步骤无异常，计算流水线总耗时
        total_elapsed = time.time() - total_start
        # 打印完整流水线执行汇总报告
        self._print_summary(total_elapsed)
        return True

    def _run_step(self, step: str) -> bool:
        """私有分发函数：根据步骤名称分发到对应子流程执行函数
        :param step: str 步骤标识字符串 prepare/train/evaluate/inference
        :return: bool 对应子流程执行成功状态
        """
        if step == 'prepare':
            return self._run_prepare()
        elif step == 'train':
            return self._run_train()
        elif step == 'evaluate':
            return self._run_evaluate()
        elif step == 'inference':
            return self._run_inference()
        else:
            # 理论不会走到此处，外层参数解析已做合法性校验
            logger.error(f"未知非法步骤标识: {step}")
            return False

    def _run_prepare(self) -> bool:
        """私有子流程：执行数据准备 prepare_data.py
        导入DataPreparer类，传入全局配置执行数据集构建
        :return: bool 数据准备是否成功
        """
        try:
            # 动态导入同目录下数据准备脚本核心类
            from prepare_data import DataPreparer
            # 实例化数据准备器，共用全局配置
            preparer = DataPreparer(self.config)
            # 执行完整数据准备流程
            return preparer.prepare()
        except Exception as e:
            # 捕获所有异常，打印完整堆栈信息，避免流水线直接崩溃
            logger.error(f"数据准备流程运行异常: {e}", exc_info=True)
            return False

    def _run_train(self) -> bool:
        """私有子流程：执行模型训练 train_model.py
        导入ModelTrainer，使用全局配置启动YOLO训练
        :return: bool 训练流程是否完整执行无报错
        """
        try:
            from train_model import ModelTrainer
            trainer = ModelTrainer(self.config)
            return trainer.train()
        except Exception as e:
            logger.error(f"模型训练流程运行异常: {e}", exc_info=True)
            return False

    def _run_evaluate(self) -> bool:
        """私有子流程：执行模型评估 evaluate_model.py
        导入ModelEvaluator，加载训练权重在验证集评估指标
        :return: bool 评估流程是否正常跑完
        """
        try:
            from evaluate_model import ModelEvaluator
            evaluator = ModelEvaluator(self.config)
            return evaluator.evaluate()
        except Exception as e:
            logger.error(f"模型评估流程运行异常: {e}", exc_info=True)
            return False

    def _run_inference(self) -> bool:
        """私有子流程：执行推理部署 inference.py
        导入InferenceEngine，自动加载最新模型并执行推理，默认保存JSON检测结果
        :return: bool 推理流程是否正常执行完成
        """
        try:
            from inference import InferenceEngine
            engine = InferenceEngine(self.config)
            # 先加载模型，加载失败直接返回False
            if not engine.load_model():
                return False
            # 执行推理，开启保存检测JSON结果
            return engine.run(save_json=True)
        except Exception as e:
            logger.error(f"推理部署流程运行异常: {e}", exc_info=True)
            return False

    def _print_summary(self, total_elapsed: float):
        """私有工具函数：流水线全部成功后打印完整汇总报告
        :param total_elapsed: float 整条流水线总运行耗时，单位秒
        """
        logger.info("\n" + "=" * 60)
        logger.info("流水线执行汇总报告")
        logger.info("=" * 60)
        logger.info(f"流水线总耗时: {total_elapsed:.1f}s")
        logger.info("")

        # 遍历所有执行过的步骤，打印状态与耗时
        for step, result in self.results.items():
            status = "✅ 成功" if result['success'] else "❌ 失败"
            logger.info(f"  {step:<15} {status}  ({result['elapsed']:.1f}s)")

        logger.info("")
        # 打印当前项目最新模型权重路径与数据集根目录，方便用户定位文件
        logger.info(f"训练完成权重路径: {self.config.get_latest_model() or '未生成模型'}")
        logger.info(f"数据集存放根目录: {self.config.project.base_dir}")


def parse_args():
    """解析终端传入的命令行参数，支持全局覆盖配置、自定义执行步骤
    :return: args 对象，存储所有命令行输入参数
    """
    # 创建参数解析器，添加脚本功能描述
    p = argparse.ArgumentParser(description='YOLO目标检测一键自动化流水线总控脚本')
    # 指定外部yaml配置文件路径
    p.add_argument('--config', default=None, help='项目yaml配置文件路径')
    # 指定待执行步骤，支持all/单步骤/逗号分隔多步骤
    p.add_argument('--step', default='all',
                   help='执行步骤: all / prepare / train / evaluate / inference (多步骤用逗号分隔)')
    # 全局覆盖原始图片目录参数
    p.add_argument('--raw_dir', default=None, help='原始图片存放目录')
    # 全局覆盖数据集输出根目录
    p.add_argument('--base_dir', default=None, help='处理后数据集输出根目录')
    # 全局覆盖预训练模型权重
    p.add_argument('--model', default=None, help='训练使用的预训练pt模型')
    # 全局覆盖训练总轮数
    p.add_argument('--epochs', type=int, default=None, help='训练总轮数')
    # 全局覆盖训练批次大小
    p.add_argument('--batch', type=int, default=None, help='训练batch批次大小')
    # 全局覆盖推理/评估置信度阈值
    p.add_argument('--conf', type=float, default=None, help='检测置信度过滤阈值(0~1)')
    # 全局覆盖推理输入源（摄像头/图片/视频/文件夹）
    p.add_argument('--source', default=None, help='推理输入源路径或摄像头编号')
    return p.parse_args()


def main():
    """程序主入口函数
    执行逻辑：
    1. 解析终端命令行参数
    2. 加载yaml全局配置
    3. 使用命令行参数覆盖配置文件对应字段（命令行优先级更高）
    4. 解析校验用户输入的执行步骤列表
    5. 实例化流水线调度器并启动完整流程
    6. 根据流水线执行状态返回程序退出码
    :return: int 0=全部步骤成功；1=参数非法/步骤执行失败
    """
    # 获取终端传入的所有参数
    args = parse_args()

    # 加载项目配置文件，无--config则读取默认配置
    config = PipelineConfig(args.config)

    # 命令行参数优先级高于yaml配置，全局覆盖对应配置字段，自动透传给下游4个子脚本
    if args.raw_dir:
        config.data.raw_dir = args.raw_dir
    if args.base_dir:
        config.project.base_dir = args.base_dir
    if args.model:
        config.training.model = args.model
    if args.epochs:
        config.training.epochs = args.epochs
    if args.batch:
        config.training.batch_size = args.batch
    if args.conf:
        config.inference.conf_threshold = args.conf
    if args.source:
        config.inference.source = args.source

    # 解析用户指定的执行步骤列表
    if args.step == 'all':
        # all代表执行全部4个步骤，使用固定顺序常量
        steps = STEPS
    else:
        # 逗号分割多步骤字符串，去除每个步骤前后空格
        steps = [s.strip() for s in args.step.split(',')]
        # 循环校验每个步骤是否为合法步骤
        for s in steps:
            if s not in STEPS:
                logger.error(f"输入非法步骤: {s}，可选步骤: {', '.join(STEPS)}")
                # 参数校验失败，直接退出程序，返回异常码1
                return 1

    # 实例化流水线调度器，传入全局配置与待执行步骤
    runner = PipelineRunner(config, steps)
    # 启动流水线，获取整体执行状态
    success = runner.run()

    # 全部步骤成功返回0，存在失败步骤返回1
    return 0 if success else 1


# 程序入口判断：仅直接运行本脚本时执行main，作为模块导入时不自动启动流水线
if __name__ == "__main__":
    # 接收main返回的状态码，安全退出程序
    sys.exit(main())