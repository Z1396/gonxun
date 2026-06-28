# YOLO 物块识别流程

完整的 YOLOv8 物块识别流水线，包含数据准备、模型训练、评估和推理部署。

## 快速开始

### 1. 安装依赖

```bash
pip install ultralytics pyyaml opencv-python numpy
```

### 2. 准备数据

将原始图片放入 `raw_images/` 目录，标注文件（JSON或txt）放在同一目录。

```
raw_images/
├── image1.jpg
├── image1.json          # LabelMe 格式标注（可选）
├── image2.jpg
├── image2.txt           # YOLO 格式标注（可选）
└── ...
```

### 3. 修改配置

编辑 `config.yaml`，设置类别名称、训练参数等。

### 4. 一键运行

```bash
# 全流水线：数据准备 → 训练 → 评估 → 推理
python run_pipeline.py

# 分步执行
python run_pipeline.py --step prepare    # 仅数据准备
python run_pipeline.py --step train      # 仅训练
python run_pipeline.py --step evaluate   # 仅评估
python run_pipeline.py --step inference  # 仅推理

# 组合步骤
python run_pipeline.py --step prepare,train
```

## 文件结构

```
yolo_pipeline/
├── config.yaml           # 配置文件（修改这里即可）
├── config_loader.py      # 配置加载器
├── prepare_data.py       # 数据准备脚本
├── train_model.py        # 模型训练脚本
├── evaluate_model.py     # 模型评估脚本
├── inference.py          # 推理部署脚本
└── run_pipeline.py       # 一键运行脚本
```

## 各脚本详细说明

### prepare_data.py - 数据准备

```bash
# 使用默认配置
python prepare_data.py

# 指定原始图片目录
python prepare_data.py --raw_dir ./my_images

# 启用数据增强
python prepare_data.py --augment

# 调整训练集比例
python prepare_data.py --train_ratio 0.9
```

**功能**：
- 收集原始图片
- 自动划分训练集/验证集
- 支持 LabelMe JSON 和 YOLO txt 标注转换
- 数据增强（翻转、亮度、对比度）
- 生成 `data.yaml`

### train_model.py - 模型训练

```bash
# 使用默认配置
python train_model.py

# 覆盖参数
python train_model.py --epochs 200 --model yolov8s.pt --batch 32

# 使用CPU训练
python train_model.py --device cpu
```

**功能**：
- 加载预训练模型（迁移学习）
- 自动读取 data.yaml
- 训练过程可视化
- 保存最佳权重

### evaluate_model.py - 模型评估

```bash
# 评估最新模型
python evaluate_model.py

# 指定模型
python evaluate_model.py --model runs/detect/exp/weights/best.pt

# 调整阈值
python evaluate_model.py --conf 0.3 --iou 0.5
```

**功能**：
- 计算 mAP50、mAP50-95、Precision、Recall
- 逐类别详细分析
- 生成混淆矩阵、PR曲线
- 对比目标指标

### inference.py - 推理部署

```bash
# 摄像头实时推理
python inference.py

# 单张图片
python inference.py --source image.jpg

# 视频文件
python inference.py --source video.mp4

# 图片目录批量推理
python inference.py --source ./test_images/

# 导出模型
python inference.py --export onnx
```

**功能**：
- 支持摄像头/图片/视频/目录
- 实时FPS显示
- 结果保存（图片/视频/JSON）
- 模型导出（ONNX/TensorRT等）

### run_pipeline.py - 一键运行

```bash
# 全流水线
python run_pipeline.py

# 指定原始图片目录
python run_pipeline.py --raw_dir ./my_images --step prepare,train

# 调整训练参数
python run_pipeline.py --epochs 200 --batch 32
```

## 配置说明

编辑 `config.yaml` 即可调整所有参数：

```yaml
classes:
  count: 6                    # 类别数量
  names:                      # 类别名称
    - "red_block"
    - "blue_block"
    # ...

training:
  model: "yolov8n.pt"         # 预训练模型
  epochs: 100                 # 训练轮数
  batch_size: 16              # 批次大小
  imgsz: 640                  # 图像尺寸
  device: 0                   # GPU设备
```

## 输出目录

```
yolo_dataset/                 # 数据集
├── images/
│   ├── train/
│   └── val/
├── labels/
│   ├── train/
│   └── val/
└── data.yaml

runs/                         # 训练输出
└── detect/
    └── exp/
        ├── weights/
        │   ├── best.pt
        │   └── last.pt
        ├── results.png
        └── confusion_matrix.png
```
