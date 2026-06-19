#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
一键运行脚本
功能：按顺序执行 数据准备 → 模型训练 → 模型评估 → 推理部署
支持分步执行和全流水线执行

使用方式:
  python run_pipeline.py                     # 全流水线执行
  python run_pipeline.py --step prepare      # 仅数据准备
  python run_pipeline.py --step train        # 仅训练
  python run_pipeline.py --step evaluate     # 仅评估
  python run_pipeline.py --step inference    # 仅推理
  python run_pipeline.py --step prepare,train  # 数据准备+训练
  python run_pipeline.py --epochs 200        # 覆盖训练轮数
  python run_pipeline.py --raw_dir ./images  # 指定原始图片目录
"""
import argparse
import logging
import sys
import time
from pathlib import Path

# 确保当前目录在路径中
sys.path.insert(0, str(Path(__file__).parent))

from config_loader import PipelineConfig

logging.basicConfig(level=logging.INFO, format='%(asctime)s [%(levelname)s] %(message)s')
logger = logging.getLogger(__name__)

STEPS = ['prepare', 'train', 'evaluate', 'inference']


class PipelineRunner:
    """流水线运行器"""

    def __init__(self, config: PipelineConfig, steps: list):
        self.config = config
        self.steps = steps
        self.results = {}

    def run(self) -> bool:
        """执行流水线"""
        logger.info("=" * 60)
        logger.info("YOLO 物块识别流水线")
        logger.info("=" * 60)
        logger.info(f"配置: {self.config}")
        logger.info(f"步骤: {self.steps}")
        logger.info("")

        total_start = time.time()

        for step in self.steps:
            step_start = time.time()
            logger.info(f"\n{'='*40}")
            logger.info(f"步骤: {step.upper()}")
            logger.info(f"{'='*40}")

            success = self._run_step(step)
            elapsed = time.time() - step_start

            self.results[step] = {
                'success': success,
                'elapsed': elapsed
            }

            if success:
                logger.info(f"✅ {step} 完成 ({elapsed:.1f}s)")
            else:
                logger.error(f"❌ {step} 失败 ({elapsed:.1f}s)")
                logger.warning("流水线中断")
                return False

        total_elapsed = time.time() - total_start
        self._print_summary(total_elapsed)
        return True

    def _run_step(self, step: str) -> bool:
        """执行单个步骤"""
        if step == 'prepare':
            return self._run_prepare()
        elif step == 'train':
            return self._run_train()
        elif step == 'evaluate':
            return self._run_evaluate()
        elif step == 'inference':
            return self._run_inference()
        else:
            logger.error(f"未知步骤: {step}")
            return False

    def _run_prepare(self) -> bool:
        """数据准备"""
        try:
            from prepare_data import DataPreparer
            preparer = DataPreparer(self.config)
            return preparer.prepare()
        except Exception as e:
            logger.error(f"数据准备失败: {e}", exc_info=True)
            return False

    def _run_train(self) -> bool:
        """模型训练"""
        try:
            from train_model import ModelTrainer
            trainer = ModelTrainer(self.config)
            return trainer.train()
        except Exception as e:
            logger.error(f"训练失败: {e}", exc_info=True)
            return False

    def _run_evaluate(self) -> bool:
        """模型评估"""
        try:
            from evaluate_model import ModelEvaluator
            evaluator = ModelEvaluator(self.config)
            return evaluator.evaluate()
        except Exception as e:
            logger.error(f"评估失败: {e}", exc_info=True)
            return False

    def _run_inference(self) -> bool:
        """推理部署"""
        try:
            from inference import InferenceEngine
            engine = InferenceEngine(self.config)
            if not engine.load_model():
                return False
            return engine.run(save_json=True)
        except Exception as e:
            logger.error(f"推理失败: {e}", exc_info=True)
            return False

    def _print_summary(self, total_elapsed: float):
        """打印执行摘要"""
        logger.info("\n" + "=" * 60)
        logger.info("流水线执行摘要")
        logger.info("=" * 60)
        logger.info(f"总耗时: {total_elapsed:.1f}s")
        logger.info("")

        for step, result in self.results.items():
            status = "✅ 成功" if result['success'] else "❌ 失败"
            logger.info(f"  {step:<15} {status}  ({result['elapsed']:.1f}s)")

        logger.info("")
        logger.info(f"模型位置: {self.config.get_latest_model() or '未找到'}")
        logger.info(f"数据集: {self.config.project.base_dir}")


def parse_args():
    p = argparse.ArgumentParser(description='YOLO 物块识别一键流水线')
    p.add_argument('--config', default=None, help='配置文件路径')
    p.add_argument('--step', default='all',
                   help='执行步骤: all / prepare / train / evaluate / inference '
                        '(可用逗号分隔多个步骤)')
    p.add_argument('--raw_dir', default=None, help='原始图片目录')
    p.add_argument('--base_dir', default=None, help='数据集输出目录')
    p.add_argument('--model', default=None, help='预训练模型')
    p.add_argument('--epochs', type=int, default=None, help='训练轮数')
    p.add_argument('--batch', type=int, default=None, help='批次大小')
    p.add_argument('--conf', type=float, default=None, help='置信度阈值')
    p.add_argument('--source', default=None, help='推理输入源')
    return p.parse_args()


def main():
    args = parse_args()

    config = PipelineConfig(args.config)

    # 命令行参数覆盖
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

    # 解析步骤
    if args.step == 'all':
        steps = STEPS
    else:
        steps = [s.strip() for s in args.step.split(',')]
        for s in steps:
            if s not in STEPS:
                logger.error(f"无效步骤: {s} (可选: {', '.join(STEPS)})")
                return 1

    runner = PipelineRunner(config, steps)
    success = runner.run()

    return 0 if success else 1


if __name__ == "__main__":
    sys.exit(main())
