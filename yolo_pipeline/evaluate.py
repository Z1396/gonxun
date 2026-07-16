#!/usr/bin/env python3
"""
YOLO 模型独立评估脚本
功能：评估训练好的模型在验证集上的性能指标
输出：mAP50、mAP50-95、Precision、Recall、混淆矩阵

使用方法：
    python evaluate.py                                    # 使用默认 best.pt
    python evaluate.py --model path/to/best.pt           # 指定模型
    python evaluate.py --data path/to/data.yaml          # 指定数据集配置
    python evaluate.py --conf 0.5 --iou 0.45             # 调整阈值
    python evaluate.py --save-json                        # 保存 JSON 报告
"""

import argparse
import json
import os
import sys
from pathlib import Path
from datetime import datetime


def find_model():
    """自动查找最新训练的模型"""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    runs_dir = os.path.join(script_dir, "runs", "detect", "runs")

    if not os.path.exists(runs_dir):
        return None

    best_models = []
    for root, dirs, files in os.walk(runs_dir):
        if "best.pt" in files:
            best_models.append(os.path.join(root, "best.pt"))

    if not best_models:
        return None

    # 返回最新修改的模型
    best_models.sort(key=lambda x: os.path.getmtime(x), reverse=True)
    return best_models[0]


def evaluate_model(model_path, data_yaml, conf=0.5, iou=0.45, imgsz=640, device=0):
    """执行模型评估"""
    from ultralytics import YOLO

    print("=" * 60)
    print("YOLO 模型评估")
    print("=" * 60)
    print(f"模型: {model_path}")
    print(f"数据集: {data_yaml}")
    print(f"置信度阈值: {conf}")
    print(f"IoU 阈值: {iou}")
    print(f"图像尺寸: {imgsz}")
    print(f"设备: {device}")
    print("=" * 60)

    # 加载模型
    model = YOLO(model_path)

    # 执行验证
    results = model.val(
        data=data_yaml,
        conf=conf,
        iou=iou,
        imgsz=imgsz,
        device=device,
        verbose=True,
    )

    return results


def generate_report(results, model_path, output_dir="."):
    """生成评估报告"""
    report = {
        "timestamp": datetime.now().strftime("%Y-%m-%d %H:%M:%S"),
        "model": model_path,
        "metrics": {},
    }

    # 提取关键指标
    try:
        report["metrics"]["mAP50"] = round(float(results.box.map50), 4)
        report["metrics"]["mAP50-95"] = round(float(results.box.map), 4)
        report["metrics"]["precision"] = round(float(results.box.mp), 4)
        report["metrics"]["recall"] = round(float(results.box.mr), 4)

        # 每个类别的指标
        names = results.names
        per_class = {}
        for i, name in names.items():
            if i < len(results.box.maps):
                per_class[name] = round(float(results.box.maps[i]), 4)
        report["metrics"]["per_class"] = per_class
    except Exception as e:
        report["error"] = str(e)

    # 打印报告
    print("\n" + "=" * 60)
    print("评估报告")
    print("=" * 60)
    print(f"时间: {report['timestamp']}")
    print(f"模型: {report['model']}")
    print("-" * 60)
    print(f"mAP50:      {report['metrics'].get('mAP50', 'N/A')}")
    print(f"mAP50-95:   {report['metrics'].get('mAP50-95', 'N/A')}")
    print(f"Precision:  {report['metrics'].get('precision', 'N/A')}")
    print(f"Recall:     {report['metrics'].get('recall', 'N/A')}")
    print("-" * 60)
    print("各类别 mAP50:")
    for name, value in report["metrics"].get("per_class", {}).items():
        print(f"  {name}: {value}")
    print("=" * 60)

    # 评估是否达标
    map50 = report["metrics"].get("mAP50", 0)
    if map50 >= 0.90:
        print(f"[通过] mAP50={map50:.4f} >= 0.90（达标）")
    elif map50 >= 0.75:
        print(f"[警告] mAP50={map50:.4f} 在 0.75-0.90 之间（建议优化）")
    else:
        print(f"[失败] mAP50={map50:.4f} < 0.75（需重新训练）")

    return report


def save_report(report, output_path):
    """保存 JSON 报告"""
    with open(output_path, "w", encoding="utf-8") as f:
        json.dump(report, f, ensure_ascii=False, indent=2)
    print(f"\n报告已保存: {output_path}")


def main():
    parser = argparse.ArgumentParser(description="YOLO 模型评估脚本")
    parser.add_argument("--model", help="模型文件路径 (.pt 或 .engine)")
    parser.add_argument("--data", help="数据集配置文件路径 (data.yaml)")
    parser.add_argument("--conf", type=float, default=0.5, help="置信度阈值")
    parser.add_argument("--iou", type=float, default=0.45, help="IoU 阈值")
    parser.add_argument("--imgsz", type=int, default=640, help="图像尺寸")
    parser.add_argument("--device", default=0, help="评估设备 (0=cuda:0, cpu)")
    parser.add_argument("--save-json", action="store_true", help="保存 JSON 报告")
    parser.add_argument("--output", default="evaluation_report.json", help="报告输出路径")
    args = parser.parse_args()

    # 查找模型
    model_path = args.model
    if not model_path:
        model_path = find_model()
        if model_path:
            print(f"[INFO] 自动找到模型: {model_path}")
        else:
            print("[错误] 未找到模型，请使用 --model 指定")
            sys.exit(1)

    if not os.path.exists(model_path):
        print(f"[错误] 模型文件不存在: {model_path}")
        sys.exit(1)

    # 查找数据集配置
    data_yaml = args.data
    if not data_yaml:
        script_dir = os.path.dirname(os.path.abspath(__file__))
        data_yaml = os.path.join(script_dir, "..", "yolo_dataset", "data.yaml")
        if not os.path.exists(data_yaml):
            print(f"[错误] 数据集配置不存在: {data_yaml}")
            sys.exit(1)

    # 执行评估
    results = evaluate_model(
        model_path, data_yaml,
        conf=args.conf, iou=args.iou,
        imgsz=args.imgsz, device=args.device
    )

    # 生成报告
    report = generate_report(results, model_path)

    # 保存报告
    if args.save_json:
        save_report(report, args.output)


if __name__ == "__main__":
    main()
