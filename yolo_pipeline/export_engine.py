#!/usr/bin/env python3
"""
TensorRT Engine 导出脚本
功能：将 YOLO 训练模型 (.pt) 转换为 TensorRT Engine (.engine)
运行环境：Jetson Nano B01 + JetPack 4.6 + TensorRT 8.2+

使用方法：
    python export_engine.py                          # 使用默认 best.pt
    python export_engine.py --model path/to/best.pt  # 指定模型路径
    python export_engine.py --fp16                    # FP16 半精度（推荐）
    python export_engine.py --int8                    # INT8 量化（速度最快）
"""

import argparse
import os
import sys
import subprocess
from pathlib import Path


def check_environment():
    """检查运行环境是否满足要求"""
    print("=" * 50)
    print("环境检查")
    print("=" * 50)

    # 检查是否在 Jetson 平台
    if not os.path.exists("/etc/nv_tegra_release"):
        print("[警告] 未检测到 Jetson 设备！")
        print("       TensorRT Engine 只能在 Jetson 平台生成")
        return False
    print("[OK] 检测到 Jetson 设备")

    # 检查 Python 版本
    print(f"[OK] Python: {sys.version.split()[0]}")

    # 检查 PyTorch
    try:
        import torch
        print(f"[OK] PyTorch: {torch.__version__}")
        if torch.cuda.is_available():
            print(f"[OK] CUDA: {torch.version.cuda}")
        else:
            print("[警告] CUDA 不可用")
            return False
    except ImportError:
        print("[错误] PyTorch 未安装")
        return False

    # 检查 TensorRT
    try:
        import tensorrt as trt
        print(f"[OK] TensorRT: {trt.__version__}")
    except ImportError:
        print("[警告] TensorRT Python 包未安装，将使用 trtexec 命令行工具")

    # 检查 ultralytics
    try:
        import ultralytics
        print(f"[OK] ultralytics: {ultralytics.__version__}")
    except ImportError:
        print("[错误] ultralytics 未安装，请运行: pip install ultralytics")
        return False

    return True


def export_with_ultralytics(model_path, fp16=True, int8=False, imgsz=640):
    """方法1：使用 ultralytics 内置导出（推荐）"""
    from ultralytics import YOLO

    print("\n" + "=" * 50)
    print("方法1：使用 ultralytics 导出")
    print("=" * 50)

    model = YOLO(model_path)

    export_args = {
        "format": "engine",
        "imgsz": imgsz,
        "device": 0,
        "dynamic": False,
        "simplify": True,
    }

    if int8:
        export_args["int8"] = True
        print("[INFO] 使用 INT8 量化")
    elif fp16:
        export_args["half"] = True
        print("[INFO] 使用 FP16 半精度")

    print(f"[INFO] 模型: {model_path}")
    print(f"[INFO] 图像尺寸: {imgsz}")

    output_path = model.export(**export_args)
    print(f"\n[成功] Engine 文件已生成: {output_path}")
    return output_path


def export_with_trtexec(onnx_path, fp16=True, int8=False, imgsz=640):
    """方法2：使用 trtexec 命令行工具"""
    print("\n" + "=" * 50)
    print("方法2：使用 trtexec 导出")
    print("=" * 50)

    engine_path = onnx_path.replace(".onnx", ".engine")

    cmd = [
        "trtexec",
        f"--onnx={onnx_path}",
        f"--saveEngine={engine_path}",
        f"--shapes=images:1x3x{imgsz}x{imgsz}",
    ]

    if fp16:
        cmd.append("--fp16")
        print("[INFO] 使用 FP16 半精度")
    if int8:
        cmd.append("--int8")
        print("[INFO] 使用 INT8 量化")

    print(f"[INFO] ONNX: {onnx_path}")
    print(f"[INFO] Engine: {engine_path}")
    print(f"[INFO] 执行: {' '.join(cmd)}")

    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode == 0:
        print(f"\n[成功] Engine 文件已生成: {engine_path}")
        return engine_path
    else:
        print(f"[错误] trtexec 执行失败:")
        print(result.stderr[-500:] if result.stderr else "无错误输出")
        return None


def verify_engine(engine_path, test_image=None):
    """验证生成的 Engine 文件"""
    print("\n" + "=" * 50)
    print("验证 Engine 文件")
    print("=" * 50)

    if not os.path.exists(engine_path):
        print(f"[错误] Engine 文件不存在: {engine_path}")
        return False

    file_size = os.path.getsize(engine_path) / (1024 * 1024)
    print(f"[OK] 文件大小: {file_size:.2f} MB")

    try:
        from ultralytics import YOLO
        model = YOLO(engine_path)

        if test_image and os.path.exists(test_image):
            print(f"[INFO] 测试图片: {test_image}")
            results = model(test_image, verbose=False)
            for r in results:
                boxes = r.boxes
                if len(boxes) > 0:
                    print(f"[OK] 检测到 {len(boxes)} 个目标")
                    for box in boxes:
                        cls = int(box.cls[0])
                        conf = float(box.conf[0])
                        print(f"     - 类别 {cls}, 置信度 {conf:.2f}")
                else:
                    print("[OK] 未检测到目标（模型加载正常）")
        else:
            print("[OK] 模型加载成功（无测试图片，跳过推理测试）")

        return True
    except Exception as e:
        print(f"[错误] 验证失败: {e}")
        return False


def main():
    parser = argparse.ArgumentParser(description="YOLO 模型导出 TensorRT Engine")
    parser.add_argument(
        "--model",
        default="runs/detect/runs/material_detection-4/weights/best.pt",
        help="模型文件路径 (.pt 或 .onnx)",
    )
    parser.add_argument("--fp16", action="store_true", default=True, help="使用 FP16 半精度（默认）")
    parser.add_argument("--int8", action="store_true", help="使用 INT8 量化")
    parser.add_argument("--imgsz", type=int, default=640, help="输入图像尺寸")
    parser.add_argument("--test-image", help="测试图片路径（可选）")
    parser.add_argument("--method", choices=["ultralytics", "trtexec"], default="ultralytics",
                        help="导出方法")
    args = parser.parse_args()

    # 环境检查
    if not check_environment():
        print("\n[错误] 环境检查未通过，请在 Jetson Nano 上运行此脚本")
        sys.exit(1)

    model_path = args.model
    if not os.path.isabs(model_path):
        script_dir = os.path.dirname(os.path.abspath(__file__))
        model_path = os.path.join(script_dir, model_path)

    if not os.path.exists(model_path):
        print(f"\n[错误] 模型文件不存在: {model_path}")
        print("请先运行训练: python run_pipeline.py")
        sys.exit(1)

    # 根据方法导出
    if args.method == "ultralytics" and model_path.endswith(".pt"):
        engine_path = export_with_ultralytics(
            model_path, fp16=args.fp16, int8=args.int8, imgsz=args.imgsz
        )
    elif args.method == "trtexec" or model_path.endswith(".onnx"):
        if model_path.endswith(".pt"):
            print("[INFO] 需要先导出 ONNX 格式")
            from ultralytics import YOLO
            onnx_path = YOLO(model_path).export(format="onnx", imgsz=args.imgsz)
        else:
            onnx_path = model_path
        engine_path = export_with_trtexec(
            onnx_path, fp16=args.fp16, int8=args.int8, imgsz=args.imgsz
        )
    else:
        print(f"[错误] 不支持的模型格式: {model_path}")
        sys.exit(1)

    # 验证
    if engine_path:
        verify_engine(engine_path, args.test_image)
        print("\n" + "=" * 50)
        print("导出完成！")
        print("=" * 50)
        print(f"\nEngine 文件: {engine_path}")
        print("\n下一步:")
        print("  1. 将 .engine 文件复制到项目目录")
        print("  2. 修改 vision_cpp/include/config.hpp 中的 MODEL_PATH")
        print("  3. 重新编译 C++ 视觉系统")
    else:
        print("\n[错误] 导出失败")
        sys.exit(1)


if __name__ == "__main__":
    main()
