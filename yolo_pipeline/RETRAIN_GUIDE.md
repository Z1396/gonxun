# YOLO 物料检测模型重新训练完整指南

> 适用项目：PlVV7XJQIfLyQn3vH8Ut 物料视觉识别系统
> 模型：YOLOv8n（6类物块检测）
> GPU：NVIDIA GeForce RTX 3060 Laptop (6GB)

---

## 目录

1. [数据准备](#1-数据准备)
2. [环境配置](#2-环境配置)
3. [训练参数设置](#3-训练参数设置)
4. [执行训练](#4-执行训练)
5. [训练过程监控](#5-训练过程监控)
6. [模型保存与版本控制](#6-模型保存与版本控制)
7. [训练结果验证与测试](#7-训练结果验证与测试)
8. [常见问题与注意事项](#8-常见问题与注意事项)

---

## 1. 数据准备

### 1.1 图片采集

使用 `tests/collect_materials.py` 从摄像头采集图片：

```powershell
python tests/collect_materials.py
```

- 按 **空格** 拍照，按 **q** 退出
- 图片自动保存到 `yolo_pipeline/raw_images/`
- **每个物块类别至少拍 50~100 张**，覆盖不同角度、距离、光照条件
- 摄像头自动设置曝光 -5，减少 50Hz 灯光频闪

### 1.2 数据清洗

采集完成后检查原始图片：

1. 删除模糊、过暗、过曝的废片
2. 删除没有目标物料的空场景图片
3. 确保每个类别都有足够数量（建议每类 ≥ 50 张）
4. 文件名保持 `material_时间戳.jpg` 格式

### 1.3 标注（LabelImg）

1. 安装 LabelImg：
   ```powershell
   pip install labelImg
   ```

2. 启动标注工具：
   ```powershell
   labelImg
   ```

3. LabelImg 中设置：
   - **打开目录**：`yolo_dataset/images/train`
   - **保存目录**：`yolo_dataset/labels/train`
   - **格式**：切换为 YOLO 格式（左侧面板点击 `PascalVOC` 切换为 `YOLO`）
   - **类别文件**：`yolo_dataset/labels/train/classes.txt`，内容为：
     ```
     red_block
     blue_block
     green_block
     yellow_block
     black_block
     light_blue_block
     ```

4. 标注操作：
   - `W` 键创建矩形框
   - 框选物块区域，选择对应类别
   - `Ctrl+S` 保存（自动生成 `.txt` 标注文件）
   - `D` 下一张，`A` 上一张

5. 标注文件格式说明（YOLO格式，每行一个目标）：
   ```
   类别ID  中心x  中心y  宽度  高度
   ```
   坐标值均为归一化的 0~1 浮点数（相对于图片宽高）

### 1.4 训练集/验证集划分

标注完成后，运行划分脚本将 20% 数据移入验证集：

```powershell
cd yolo_pipeline
python split_val.py
```

脚本逻辑：
- 从 `images/train` + `labels/train` 中随机取 20% 移动到 `images/val` + `labels/val`
- 只移动有对应标注文件的图片（跳过无标注的图片）

### 1.5 数据集目录结构确认

训练前确认目录结构如下：

```
yolo_dataset/
├── data.yaml                    # 数据集配置文件
├── images/
│   ├── train/                   # 训练图片（.jpg）
│   └── val/                     # 验证图片（.jpg）
└── labels/
    ├── train/                   # 训练标注（.txt）+ classes.txt
    └── val/                     # 验证标注（.txt）
```

### 1.6 data.yaml 配置

确认 `yolo_dataset/data.yaml` 内容正确：

```yaml
path: C:\Users\86182\Downloads\project\PlVV7XJQIfLyQn3vH8Ut-master-f7b1db2bd8454ae6d13b3c5da9734a0abbe446ae\yolo_dataset
train: images/train
val: images/val

nc: 6
names:
  0: red_block
  1: blue_block
  2: green_block
  3: yellow_block
  4: black_block
  5: light_blue_block
```

> **注意**：`path` 必须是绝对路径，`nc` 必须与实际类别数一致，`names` 顺序必须与 classes.txt 一致。

---

## 2. 环境配置

### 2.1 依赖安装

```powershell
# 安装 ultralytics（YOLOv8）
pip install ultralytics

# 安装 CUDA 版 PyTorch（GPU 训练必须）
pip install torch torchvision --index-url https://download.pytorch.org/whl/cu121
```

### 2.2 版本兼容性检查

在终端运行以下命令验证环境：

```powershell
python -c "import torch; print(f'PyTorch: {torch.__version__}'); print(f'CUDA可用: {torch.cuda.is_available()}'); print(f'GPU: {torch.cuda.get_device_name(0)}' if torch.cuda.is_available() else '无GPU')"
```

期望输出：
```
PyTorch: 2.4.1+cu121
CUDA可用: True
GPU: NVIDIA GeForce RTX 3060 Laptop GPU
```

### 2.3 常见环境问题

| 问题 | 原因 | 解决方法 |
|------|------|---------|
| `Torch not compiled with CUDA enabled` | 安装了CPU版PyTorch | 卸载后重装CUDA版 |
| `ultralytics未安装` | 缺少依赖 | `pip install ultralytics` |
| `MemoryError` | workers过多内存不足 | 设置 `workers: 0` |
| `PermissionError` | sandbox多进程限制 | 设置 `workers: 0` |

---

## 3. 训练参数设置

所有训练参数在 `yolo_pipeline/config.yaml` 中配置：

### 3.1 核心训练参数

| 参数 | 当前值 | 说明 | 调优建议 |
|------|--------|------|---------|
| `training.model` | `yolov8n.pt` | 预训练模型 | n=最快最小，s=稍大更准，数据少用n即可 |
| `training.epochs` | `100` | 训练总轮数 | 数据少时50-100即可，数据多可200+ |
| `training.batch_size` | `8` | 每批图片数 | RTX3060 6GB：8安全，16可能OOM |
| `training.imgsz` | `640` | 输入图像尺寸 | 640标准，物块小可试1280（显存翻倍） |
| `training.device` | `0` | GPU设备号 | 0=第一张GPU，-1=CPU |
| `training.workers` | `0` | 数据加载线程 | **必须设0**，避免sandbox权限问题 |

### 3.2 学习率参数

| 参数 | 当前值 | 说明 | 调优建议 |
|------|--------|------|---------|
| `training.lr0` | `0.01` | 初始学习率 | 默认0.01即可；loss震荡可降至0.001 |
| `training.lrf` | `0.01` | 最终学习率系数 | 最终lr = lr0 × lrf，默认即可 |

### 3.3 早停与保存

| 参数 | 当前值 | 说明 |
|------|--------|------|
| `training.patience` | `50` | 验证集mAP连续50轮无提升则停止 |
| `training.save_period` | `10` | 每10轮保存一次中间权重 |

### 3.4 模型规模选择

| 模型 | 参数量 | 适用场景 |
|------|--------|---------|
| `yolov8n.pt` | 3.0M | 数据少（<200张），速度优先 |
| `yolov8s.pt` | 11.1M | 数据中等（200-500张），精度优先 |
| `yolov8m.pt` | 25.9M | 数据丰富（500+），需要高精度 |

---

## 4. 执行训练

### 4.1 标准训练命令

```powershell
cd yolo_pipeline
python train_model.py --device 0 --model ..\yolov8n.pt --batch 8
```

### 4.2 命令行参数（覆盖config.yaml）

```powershell
# 自定义训练轮数
python train_model.py --epochs 200

# 更大模型
python train_model.py --model ..\yolov8s.pt

# 自定义批次大小
python train_model.py --batch 4

# 自定义学习率
python train_model.py --lr0 0.005

# 自定义实验名称
python train_model.py --name material_detection_v2
```

### 4.3 重新训练前的清理

如果要**完全从头训练**（不依赖旧缓存）：

```powershell
# 删除旧的数据集缓存（修改标注后必须删）
del yolo_dataset\labels\train.cache
del yolo_dataset\labels\val.cache

# 删除旧模型权重（可选，不影响新训练）
# 新训练会自动保存到 material_detection-N 目录
```

---

## 5. 训练过程监控

### 5.1 终端日志

训练过程中终端实时输出每个epoch的关键指标：

```
Epoch  GPU_mem  box_loss  cls_loss  dfl_loss  Instances  Size
21/100  2.22G    0.9595    1.033     1.089       16       640

Class  Images  Instances  Box(P     R      mAP50  mAP50-95)
all      36       36     0.994     1      0.995    0.802
```

关键指标解读：

| 指标 | 含义 | 期望范围 |
|------|------|---------|
| `box_loss` | 边界框回归损失 | 持续下降 |
| `cls_loss` | 分类损失 | 持续下降 |
| `dfl_loss` | 分布焦点损失 | 持续下降 |
| `P (Precision)` | 精确率 | >0.9 |
| `R (Recall)` | 召回率 | >0.9 |
| `mAP50` | IoU=0.5时平均精度 | >0.9 |
| `mAP50-95` | IoU=0.5~0.95平均精度 | >0.7 |

### 5.2 训练曲线图

训练结束后自动生成图表，位于：

```
yolo_pipeline/runs/detect/runs/material_detection-N/
├── results.png          # loss曲线 + mAP曲线
├── confusion_matrix.png # 混淆矩阵
├── labels.jpg           # 标签分布统计
└── ...
```

直接打开 `results.png` 查看训练趋势。

### 5.3 TensorBoard 实时监控

```powershell
cd yolo_pipeline
tensorboard --logdir runs/detect/runs
```

浏览器打开 `http://localhost:6006` 可实时查看训练曲线。

---

## 6. 模型保存与版本控制

### 6.1 自动保存规则

每次训练自动创建新目录，不会覆盖旧模型：

```
yolo_pipeline/runs/detect/runs/
├── material_detection/       # 第1次训练
├── material_detection-2/     # 第2次训练
├── material_detection-3/     # 第3次训练
└── material_detection-4/     # 第4次训练（最新）
    └── weights/
        ├── best.pt           # 验证集mAP最高的权重
        ├── last.pt           # 最后一轮权重
        ├── epoch10.pt        # 每10轮中间权重
        ├── epoch20.pt
        └── ...
```

### 6.2 权重文件说明

| 文件 | 用途 | 大小 |
|------|------|------|
| `best.pt` | 推理部署用，mAP最高 | ~6.2MB |
| `last.pt` | 断点续训用 | ~6.2MB |
| `epochN.pt` | 中间检查点 | ~6.2MB |

### 6.3 使用新模型的步骤

训练完成后，需要更新 `vision/system.py` 中的模型路径：

```python
# 找到最新训练的 best.pt 路径，替换到 system.py
self.yolo_detector = YOLOv8Detector(
    model_path='yolo_pipeline/runs/detect/runs/material_detection-N/weights/best.pt',
    conf_threshold=0.5, device=0
)
```

---

## 7. 训练结果验证与测试

### 7.1 摄像头实时测试

```powershell
cd yolo_pipeline
python inference.py --model ..\yolo_pipeline\runs\detect\runs\material_detection-N\weights\best.pt --source 0
```

- 实时显示检测框、类别、置信度、FPS
- 按 **q** 退出

### 7.2 图片批量测试

```powershell
python inference.py --model ..\yolo_pipeline\runs\detect\runs\material_detection-N\weights\best.pt --source ..\yolo_dataset\images\val
```

### 7.3 评估指标检查

训练结束后的验证结果应满足：

| 指标 | 目标值 | 当前实测 |
|------|--------|---------|
| mAP50 | ≥0.90 | 0.995 |
| mAP50-95 | ≥0.75 | 0.840 |
| Precision | ≥0.90 | 0.999 |
| Recall | ≥0.90 | 1.000 |

### 7.4 实际场景验证清单

- [ ] 各类物块都能被正确识别
- [ ] 不同距离下检测稳定
- [ ] 不同光照条件下检测稳定
- [ ] 多个物块同时出现时都能检测到
- [ ] 误检率低（不会把非物块识别为物块）
- [ ] 实时FPS ≥ 15（实际使用要求）

---

## 8. 常见问题与注意事项

### 8.1 网络问题

| 问题 | 解决方法 |
|------|---------|
| 下载 `yolov8n.pt` 超时 | 项目根目录已有本地 `yolov8n.pt`，用 `--model ..\yolov8n.pt` 指定 |
| AMP检查下载模型卡住 | `train_model.py` 已设置 `amp: False` 跳过检查 |

### 8.2 内存/显存问题

| 问题 | 解决方法 |
|------|---------|
| `MemoryError` | 设置 `workers: 0` |
| `PermissionError [WinError 5]` | 设置 `workers: 0`（sandbox多进程限制）|
| `CUDA out of memory` | 减小 `batch_size`：16→8→4 |

### 8.3 训练效果问题

| 问题 | 可能原因 | 解决方法 |
|------|---------|---------|
| mAP低（<0.5） | 标注质量差/数据太少 | 检查标注准确性，增加每类50+图片 |
| 只检测到1个类别 | 标注只覆盖部分类别 | 检查所有6类是否有标注数据 |
| 过拟合（train好val差） | 数据少且无增强 | 增加数据量，确保数据增强开启 |
| loss不下降 | 学习率过大/过小 | 调整 lr0：0.01→0.005 或 0.02 |

### 8.4 PowerShell 注意事项

PowerShell 不支持 `&&` 连接命令，使用 `;` 代替：

```powershell
# 错误
cd yolo_pipeline && python train_model.py

# 正确
cd yolo_pipeline; python train_model.py
```

### 8.5 修改标注后重新训练流程

```
1. 修改/新增标注文件
2. 删除缓存：del yolo_dataset\labels\train.cache 和 val.cache
3. 如有新增图片需重新划分：python split_val.py
4. 重新训练：python train_model.py --device 0 --model ..\yolov8n.pt --batch 8
5. 更新 system.py 中的模型路径
```
