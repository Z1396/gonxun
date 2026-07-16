# 工创赛2025智能物流搬运系统 - 部署全流程文档

> **项目名称：** gonxun（工创赛智能物流搬运视觉系统）
> **目标平台：** Jetson Nano B01 + Ubuntu 18.04 + JetPack 4.6
> **开发平台：** x86_64 Linux (Ubuntu 22.04)
> **文档版本：** v1.0
> **更新日期：** 2026-07-15

---

## 目录

1. [项目架构概览](#1-项目架构概览)
2. [开发环境准备](#2-开发环境准备)
3. [YOLO 模型训练流程](#3-yolo-模型训练流程)
4. [C++ 视觉系统编译](#4-c-视觉系统编译)
5. [Jetson Nano 部署流程](#5-jetson-nano-部署流程)
6. [系统运行与配置](#6-系统运行与配置)
7. [故障排查](#7-故障排查)

---

## 1. 项目架构概览

### 1.1 目录结构

```
gonxun/
├── main.cpp                         # 程序入口
├── CMakeLists.txt                   # 全局构建配置
├── .clangd                          # clangd 配置
├── .vscode/settings.json            # VSCode 配置
│
├── core/                            # 核心库 (libgonxun_core.so)
│   ├── include/                     # 头文件
│   │   ├── astar_planner.hpp        # A* 路径规划
│   │   ├── grid_planner.hpp         # 3×3 网格规划
│   │   ├── task_state_machine.hpp   # 任务状态机
│   │   ├── robot_controller.hpp     # 机器人控制
│   │   ├── task_simulator.hpp       # 任务仿真
│   │   ├── vision_controller.hpp    # 视觉线程管理
│   │   ├── cli_parser.hpp           # 命令行解析
│   │   └── app_signals.hpp          # 信号处理
│   └── src/                         # 源文件
│
├── vision_cpp/                      # 视觉库 (libgonxun_vision.a)
│   ├── include/                     # 头文件
│   │   ├── config.hpp               # 视觉配置
│   │   ├── vision_system.hpp        # 视觉系统主类
│   │   ├── yolo_detector.hpp        # YOLO 检测
│   │   ├── camera_manager.hpp       # 摄像头管理
│   │   ├── color_detector.hpp       # 颜色识别
│   │   ├── ring_detector.hpp        # 色环定位
│   │   ├── qr_detector.hpp          # 二维码识别
│   │   ├── obstacle_detector.hpp    # 障碍物检测
│   │   ├── serial_comm.hpp          # 串口通信
│   │   ├── kalman_filter.hpp        # 卡尔曼滤波
│   │   └── task_display.hpp         # 任务信息绘制
│   └── src/                         # 源文件
│
├── gui/                             # GUI 可执行文件 (CourtMapViewer)
│   ├── include/                     # 头文件
│   │   ├── mainwindow.h             # 主窗口
│   │   ├── courtmapwidget.h         # 场地地图控件
│   │   ├── data_panel_widget.h      # 数据面板
│   │   └── simulation_controller.h  # 仿真控制器
│   └── src/                         # 源文件
│
├── config/                          # 配置文件
│   ├── config.yaml                  # YAML 配置
│   ├── include/config_loader.hpp    # 配置加载器
│   └── src/config_loader.cpp
│
├── yolo_dataset/                    # YOLO 数据集
│   ├── data.yaml                    # 数据集配置
│   ├── images/
│   │   ├── train/                   # 训练图片 (65 张)
│   │   └── val/                     # 验证图片 (36 张)
│   └── labels/
│       ├── train/                   # 训练标签
│       └── val/                     # 验证标签
│
└── yolo_pipeline/                   # YOLO 训练流水线
    ├── config.yaml                  # 训练参数配置
    ├── config_loader.py             # 配置加载
    ├── prepare_data.py              # 数据预处理
    ├── split_val.py                 # 验证集分割
    ├── run_pipeline.py              # 训练主流程
    ├── inference.py                 # 推理测试
    ├── evaluate.py                  # 模型评估
    ├── export_engine.py             # TensorRT 导出
    ├── requirements.txt             # Python 依赖
    ├── yolo26n.pt                   # 基础预训练权重
    └── runs/                        # 训练输出
        └── detect/runs/material_detection-4/
            └── weights/
                ├── best.pt          # 最佳模型
                ├── last.pt          # 最后 epoch 模型
                └── best.onnx        # ONNX 格式
```

### 1.2 架构层次

```
┌─────────────────────────────────────────────────┐
│              CourtMapViewer (GUI)                │
│  ┌──────────┐  ┌──────────┐  ┌──────────────┐  │
│  │ 地图控件  │  │ 数据面板  │  │ 仿真控制器    │  │
│  └──────────┘  └──────────┘  └──────────────┘  │
├─────────────────────────────────────────────────┤
│              gonxun_core (核心库)                │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌─────────┐  │
│  │A*规划  │ │网格规划│ │状态机  │ │仿真器   │  │
│  └────────┘ └────────┘ └────────┘ └─────────┘  │
├─────────────────────────────────────────────────┤
│             gonxun_vision (视觉库)               │
│  ┌────────┐ ┌────────┐ ┌────────┐ ┌─────────┐  │
│  │YOLO检测│ │摄像头  │ │颜色识别│ │串口通信 │  │
│  └────────┘ └────────┘ └────────┘ └─────────┘  │
└─────────────────────────────────────────────────┘
```

---

## 2. 开发环境准备

### 2.1 开发机依赖安装

```bash
# 基础工具
sudo apt update
sudo apt install -y build-essential cmake pkg-config git

# C++ 依赖
sudo apt install -y libopencv-dev libyaml-cpp-dev

# Qt5
sudo apt install -y qtbase5-dev qttools5-dev-tools

# clangd（代码补全）
sudo apt install -y clangd
```

### 2.2 验证安装

```bash
# 验证 CMake
cmake --version  # 需要 >= 3.10

# 验证 OpenCV
pkg-config --modversion opencv4  # 需要 >= 4.5

# 验证 Qt
qmake --version  # 需要 >= 5.15

# 验证编译器
g++ --version  # 需要 >= 11.0，支持 C++17
```

### 2.3 VSCode 配置

项目已内置 `.vscode/settings.json`，确保构建目录指向 `build/`：

```json
{
    "cmake.sourceDirectory": "/home/pldx/Desktop/gonxun",
    "cmake.buildDirectory": "${workspaceFolder}/build",
    "cmake.configureOnOpen": true,
    "clangd.arguments": [
        "--compile-commands-dir=${workspaceFolder}/build",
        "--header-insertion=never",
        "--background-index"
    ]
}
```

---

## 3. YOLO 模型训练流程

### 3.1 安装 Python 依赖

```bash
cd /home/pldx/Desktop/gonxun/yolo_pipeline
pip install -r requirements.txt
```

> **注意：** Jetson Nano 需要安装 NVIDIA 提供的专用 PyTorch wheel，参见 [第 5 节](#5-jetson-nano-部署流程)。

### 3.2 数据集准备

#### 3.2.1 数据集结构

```
yolo_dataset/
├── data.yaml                # 配置文件
├── images/
│   ├── train/  (*.jpg)      # 训练图片
│   └── val/    (*.jpg)      # 验证图片
└── labels/
    ├── train/  (*.txt)      # YOLO 格式标签
    └── val/    (*.txt)
```

#### 3.2.2 标签格式

每个 `.txt` 文件对应一张图片，每行一个目标：

```
<class_id> <cx> <cy> <width> <height>
```

- 坐标均为归一化值 (0-1)
- `class_id` 对应 `data.yaml` 中的类别索引

#### 3.2.3 类别定义

| ID | 类别名 | 说明 |
|----|--------|------|
| 0 | red_block | 红色物料 |
| 1 | blue_block | 蓝色物料 |
| 2 | green_block | 绿色物料 |
| 3 | yellow_block | 黄色物料 |
| 4 | black_block | 黑色物料 |
| 5 | light_blue_block | 浅蓝色物料 |

#### 3.2.4 添加新数据

```bash
# 1. 将新图片放入 raw_images/
cp /path/to/new_images/*.jpg yolo_pipeline/raw_images/

# 2. 运行数据预处理
cd yolo_pipeline
python prepare_data.py

# 3. 重新分割训练/验证集
python split_val.py
```

### 3.3 训练参数配置

编辑 `yolo_pipeline/config.yaml`：

```yaml
training:
  model: "yolov8n.pt"      # 预训练模型 (n/s/m/l/x)
  epochs: 100               # 训练轮数
  batch_size: 8             # 批次大小（根据显存调整）
  imgsz: 640                # 图像尺寸
  device: 0                 # GPU 设备
  workers: 0                # 数据加载线程（Jetson 建议 0）
  lr0: 0.01                 # 初始学习率
  patience: 50              # 早停轮数
```

### 3.4 启动训练

```bash
cd /home/pldx/Desktop/gonxun/yolo_pipeline
python run_pipeline.py
```

训练输出位于 `yolo_pipeline/runs/detect/runs/material_detection-X/`。

### 3.5 模型评估

```bash
# 默认评估（自动查找最新模型）
python evaluate.py

# 指定模型和阈值
python evaluate.py --model runs/detect/runs/material_detection-4/weights/best.pt \
                   --conf 0.5 --iou 0.45

# 保存 JSON 报告
python evaluate.py --save-json
```

**达标标准：**

| 指标 | 目标值 | 说明 |
|------|--------|------|
| mAP50 | ≥ 0.90 | 通过 |
| mAP50 | 0.75-0.90 | 建议优化 |
| mAP50 | < 0.75 | 需重新训练 |
| mAP50-95 | ≥ 0.75 | 通过 |
| Precision | ≥ 0.90 | 通过 |
| Recall | ≥ 0.85 | 通过 |

### 3.6 推理测试

```bash
# 测试单张图片
python inference.py --source path/to/image.jpg

# 测试摄像头
python inference.py --source 0
```

---

## 4. C++ 视觉系统编译

### 4.1 编译前配置

检查 `vision_cpp/include/config.hpp` 中的关键配置：

```cpp
// 模型路径（训练完成后修改为实际路径）
#define MODEL_PATH "yolo_pipeline/runs/detect/runs/material_detection-4/weights/best.pt"

// 摄像头索引（根据实际连接修改）
#define CAMERA_MAIN_INDEX 0
#define CAMERA_QR_INDEX 1

// 串口配置
#define SERIAL_MOCK false        // 比赛时设为 false
#define SERIAL_PORT "/dev/ttyUSB0"
#define SERIAL_BAUDRATE 115200
```

### 4.2 编译步骤

```bash
cd /home/pldx/Desktop/gonxun

# 1. 创建构建目录
mkdir -p build && cd build

# 2. CMake 配置
cmake ..

# 3. 编译（4 线程并行）
make -j4

# 4. 验证产物
ls -la bin/ lib/
```

### 4.3 编译产物

```
build/
├── bin/
│   └── CourtMapViewer          # GUI 可执行文件
└── lib/
    ├── libgonxun_core.so       # 核心动态库
    └── libgonxun_vision.a      # 视觉静态库
```

### 4.4 编译选项

```bash
# Debug 模式（带调试信息）
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Release 模式（优化性能）
cmake .. -DCMAKE_BUILD_TYPE=Release

# 禁用 GUI（仅命令行）
cmake .. -DENABLE_GUI=OFF

# 启用 CUDA（Jetson 平台）
cmake .. -DENABLE_CUDA=ON
```

### 4.5 VSCode 编译

按 `F7` 即可编译，构建目录已配置为 `${workspaceFolder}/build`。

---

## 5. Jetson Nano 部署流程

### 5.1 系统准备

```bash
# 1. 开启最大性能模式
sudo jetson_clocks

# 2. 安装系统依赖
sudo apt update
sudo apt install -y build-essential cmake pkg-config
sudo apt install -y libopencv-dev libyaml-cpp-dev
sudo apt install -y qtbase5-dev qttools5-dev-tools
sudo apt install -y v4l-utils udev usbutils
```

### 5.2 安装 PyTorch（Jetson 专用）

```bash
# JetPack 4.6 对应 PyTorch 1.10.0
# 下载地址: https://developer.download.nvidia.com/compute/redist/jp/v461/pytorch/

# 安装 PyTorch wheel
pip install torch-1.10.0+nv21.12-cp38-cp38-linux_aarch64.whl
pip install torchvision==0.11.1

# 验证
python -c "import torch; print(torch.__version__); print(torch.cuda.is_available())"
```

### 5.3 安装 Python 依赖

```bash
cd /home/pldx/Desktop/gonxun/yolo_pipeline
pip install -r requirements.txt
```

### 5.4 配置硬件权限

```bash
# 串口权限（CH341）
echo 'SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", MODE="0666", SYMLINK+="ttyCH341USB0"' | \
    sudo tee /etc/udev/rules.d/99-ch341.rules
sudo udevadm control --reload-rules
sudo udevadm trigger

# 用户组权限
sudo usermod -aG dialout $USER
sudo usermod -aG video $USER
sudo usermod -aG tty $USER

# 重新登录使权限生效
exit
# 重新登录后继续
```

### 5.5 检测硬件设备

```bash
# 检测摄像头
v4l2-ctl --list-devices
# 输出示例:
# /dev/video0 (主摄像头)
# /dev/video1 (二维码摄像头)

# 检测串口
ls /dev/ttyUSB* /dev/ttyACM*
# 输出示例:
# /dev/ttyUSB0
```

### 5.6 同步项目到 Jetson

```bash
# 方法1: 使用 scp 从开发机传输
scp -r /home/pldx/Desktop/gonxun user@jetson-ip:~/gonxun

# 方法2: 使用 git
cd /home/pldx/Desktop/gonxun
git add .
git commit -m "Deploy to Jetson"
git push origin main

# 在 Jetson 上
git clone https://github.com/Z1396/gonxun.git ~/gonxun
```

### 5.7 Jetson 上编译

```bash
cd ~/gonxun
mkdir -p build && cd build
cmake .. -DENABLE_CUDA=ON
make -j4
```

### 5.8 导出 TensorRT Engine

> **重要：** TensorRT Engine 必须在 Jetson 上生成，不能跨平台使用。

```bash
cd ~/gonxun/yolo_pipeline

# 方法1: 使用 ultralytics API（推荐）
python export_engine.py --fp16

# 方法2: 使用 trtexec 命令行
python export_engine.py --method trtexec --fp16

# 方法3: 指定模型路径
python export_engine.py --model runs/detect/runs/material_detection-4/weights/best.pt --fp16
```

导出完成后，会生成 `best.engine` 文件。

### 5.9 更新 C++ 配置

修改 `vision_cpp/include/config.hpp`：

```cpp
// 使用 TensorRT Engine 文件
#define MODEL_PATH "yolo_pipeline/runs/detect/runs/material_detection-4/weights/best.engine"
```

重新编译：

```bash
cd ~/gonxun/build
make -j4
```

---

## 6. 系统运行与配置

### 6.1 运行模式

#### 6.1.1 GUI 可视化模式（开发调试）

```bash
cd ~/gonxun
./build/bin/CourtMapViewer
```

**界面操作：**
1. 点击"标记障碍物"按钮，在地图上标记障碍物
2. 点击"选择启停区"按钮，选择机器人起始位置
3. 输入任务码（如 "312"）
4. 点击"仿真"按钮开始仿真

#### 6.1.2 命令行仿真模式（无 GUI）

```bash
# 运行仿真（任务码 312）
./build/bin/CourtMapViewer --simulate --task-code 312

# 指定循环次数
./build/bin/CourtMapViewer --simulate --task-code 312 --cycles 3
```

#### 6.1.3 无头后台模式（Jetson 部署）

```bash
# 运行视觉系统（无 GUI）
./build/bin/CourtMapViewer --headless

# 指定串口
./build/bin/CourtMapViewer --headless --serial /dev/ttyUSB0

# 模拟串口模式（测试用）
./build/bin/CourtMapViewer --headless --mock-serial
```

### 6.2 命令行参数

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `--simulate` | 启动仿真模式 | - |
| `--task-code <code>` | 任务码（3位数字） | "123" |
| `--cycles <n>` | 循环次数 | 1 |
| `--headless` | 无头模式（无 GUI） | - |
| `--serial <port>` | 串口设备路径 | /dev/ttyUSB0 |
| `--mock-serial` | 模拟串口 | false |
| `--config <path>` | 配置文件路径 | config/config.yaml |

### 6.3 配置文件

编辑 `config/config.yaml`：

```yaml
# 摄像头配置
camera:
  main_index: 0          # 主摄像头索引
  qr_index: 1            # 二维码摄像头索引
  width: 640
  height: 480

# 串口配置
serial:
  port: /dev/ttyUSB0
  baudrate: 115200
  mock: false            # 比赛时设为 false

# 模型配置
yolo:
  model_path: yolo_pipeline/runs/detect/runs/material_detection-4/weights/best.engine
  conf_threshold: 0.5
  iou_threshold: 0.45
```

### 6.4 比赛部署检查清单

部署到比赛场地前，逐项确认：

- [ ] `config.hpp` 中 `SERIAL_MOCK` 设为 `false`
- [ ] `config.hpp` 中 `CAMERA_MAIN_INDEX` 和 `CAMERA_QR_INDEX` 匹配实际设备
- [ ] `config.hpp` 中 `MODEL_PATH` 指向 `.engine` 文件
- [ ] 串口权限已配置（`/dev/ttyUSB0` 可访问）
- [ ] 摄像头已连接且 `v4l2-ctl --list-devices` 能识别
- [ ] `jetson_clocks` 已开启（最大性能模式）
- [ ] 程序能正常启动无报错
- [ ] 仿真模式测试通过

---

## 7. 故障排查

### 7.1 编译问题

#### 问题：CMake 找不到 OpenCV

```bash
# 检查 OpenCV 安装
pkg-config --modversion opencv4

# 如果未安装
sudo apt install -y libopencv-dev
```

#### 问题：Qt 头文件报红（clangd）

重启 clangd 语言服务器：
- VSCode 中按 `Ctrl+Shift+P`
- 输入 `clangd: Restart language server`
- 回车

#### 问题：增量编译不生效

```bash
# 清理后重新编译
cd build
make clean && make -j4

# 或彻底重建
rm -rf build
mkdir build && cd build
cmake .. && make -j4
```

### 7.2 运行时问题

#### 问题：摄像头无法打开

```bash
# 检查设备
v4l2-ctl --list-devices

# 检查权限
ls -la /dev/video*

# 添加用户到 video 组
sudo usermod -aG video $USER
# 重新登录生效
```

#### 问题：串口无法打开

```bash
# 检查设备
ls /dev/ttyUSB* /dev/ttyACM*

# 检查权限
ls -la /dev/ttyUSB0

# 添加 udev 规则（参见 5.4 节）
```

#### 问题：YOLO 模型加载失败

```bash
# 检查模型文件是否存在
ls -la yolo_pipeline/runs/detect/runs/material_detection-4/weights/best.engine

# 如果只有 .pt 文件，需要导出 Engine
cd yolo_pipeline
python export_engine.py --fp16
```

#### 问题：TensorRT Engine 不兼容

> Engine 文件必须在目标平台上生成，不能跨平台使用。

```bash
# 在 Jetson 上重新生成
cd ~/gonxun/yolo_pipeline
python export_engine.py --fp16
```

### 7.3 性能问题

#### Jetson Nano 性能优化

```bash
# 1. 开启最大性能模式
sudo jetson_clocks

# 2. 监控资源
sudo tegrastats

# 3. 降低推理分辨率（修改 config.yaml）
yolo:
  imgsz: 320    # 从 640 降到 320，提升速度

# 4. 使用 FP16 半精度
# 确保 .engine 文件使用 --fp16 导出
```

#### 内存不足

```bash
# 检查内存
free -h

# 关闭不必要的进程
sudo systemctl stop gdm3  # 关闭桌面环境（headless 模式）

# 增加 swap
sudo fallocate -l 4G /swapfile
sudo chmod 600 /swapfile
sudo mkswap /swapfile
sudo swapon /swapfile
```

---

## 附录

### A. 完整部署命令速查

```bash
# === 开发机 ===
# 编译
cd /home/pldx/Desktop/gonxun && mkdir -p build && cd build && cmake .. && make -j4

# 训练
cd /home/pldx/Desktop/gonxun/yolo_pipeline && python run_pipeline.py

# 评估
python evaluate.py --save-json

# === Jetson Nano ===
# 安装依赖
pip install -r yolo_pipeline/requirements.txt

# 导出 Engine
cd yolo_pipeline && python export_engine.py --fp16

# 编译
cd ~/gonxun && mkdir -p build && cd build && cmake .. -DENABLE_CUDA=ON && make -j4

# 运行
./build/bin/CourtMapViewer --headless --serial /dev/ttyUSB0
```

### B. 关键文件路径速查

| 文件 | 路径 |
|------|------|
| 可执行文件 | `build/bin/CourtMapViewer` |
| 核心库 | `build/lib/libgonxun_core.so` |
| 视觉库 | `build/lib/libgonxun_vision.a` |
| 训练模型 | `yolo_pipeline/runs/detect/runs/material_detection-4/weights/best.pt` |
| Engine 文件 | `yolo_pipeline/runs/detect/runs/material_detection-4/weights/best.engine` |
| 数据集配置 | `yolo_dataset/data.yaml` |
| 训练配置 | `yolo_pipeline/config.yaml` |
| 系统配置 | `config/config.yaml` |
| C++ 配置 | `vision_cpp/include/config.hpp` |

### C. 联系方式

- **GitHub 仓库：** https://github.com/Z1396/gonxun
- **问题反馈：** 通过 GitHub Issues 提交
