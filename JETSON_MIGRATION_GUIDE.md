# Jetson Nano B01 迁移指南

> 目标环境：Jetson Nano B01 + Ubuntu 18.04 + JetPack 4.6
> 项目：工创赛2025智能物流搬运视觉系统

---

## 目录

1. [环境差异对比](#1-环境差异对比)
2. [迁移前准备](#2-迁移前准备)
3. [依赖安装](#3-依赖安装)
4. [配置修改](#4-配置修改)
5. [代码修改](#5-代码修改)
6. [硬件检测与验证](#6-硬件检测与验证)
7. [性能优化建议](#7-性能优化建议)

---

## 1. 环境差异对比

### 系统架构

| 项目 | Windows（当前） | Jetson Nano（目标） |
|------|----------------|--------------------|
| 操作系统 | Windows 10/11 | Ubuntu 18.04 LTS |
| CPU架构 | x86_64 | ARM64 (aarch64) |
| GPU | NVIDIA RTX 3060 (6GB) | NVIDIA Tegra X1 (4GB共享) |
| CUDA版本 | 12.1 | 10.2（JetPack 4.6） |
| PyTorch来源 | 官方wheel | NVIDIA专用wheel |
| 摄像头后端 | CAP_DSHOW | CAP_V4L2 |

### 硬件差异

| 项目 | Windows | Jetson Nano |
|------|---------|-------------|
| 串口设备路径 | COM3 / COM4 | `/dev/ttyUSB0` / `/dev/ttyACM0` |
| 摄像头索引 | 0/1/2（自动检测） | 用v4l2-ctl手动检测 |
| 显示输出 | GUI窗口imshow | 无GUI（headless）或远程桌面 |

---

## 2. 迁移前准备

### 2.1 Jetson Nano 系统检查

```bash
# 检查JetPack版本
cat /etc/nv_tegra_release
# 期望输出：R32 (release), REVISION: 6.1（JetPack 4.6）

# 检查CUDA版本
nvcc --version
# 期望：Cuda compilation tools, release 10.2

# 检查内存
free -h
# Jetson Nano 4GB版：Total ~4GiB

# 检查磁盘空间
df -h
# 确保至少 10GB 可用空间
```

### 2.2 创建项目目录

```bash
# 创建项目目录
mkdir -p ~/projects/vision_system
cd ~/projects/vision_system

# 从Windows复制项目文件
# 方式1：SCP传输（推荐）
scp -r user@windows_pc:/path/to/project/* ~/projects/vision_system/

# 方式2：Git克隆（如有仓库）
git clone <repo_url>
```

---

## 3. 依赖安装

### 3.1 安装系统依赖

```bash
# 更新系统
sudo apt update && sudo apt upgrade -y

# 安装基础工具
sudo apt install -y \
    build-essential cmake pkg-config \
    libjpeg-dev libpng-dev libtiff-dev \
    libavcodec-dev libavformat-dev libswscale-dev \
    libv4l-dev libxvidcore-dev libx264-dev \
    udev usbutils v4l-utils \
    python3-dev python3-pip python3-venv
```

### 3.2 创建虚拟环境

```bash
# Jetson Nano 使用 Python 3.6（系统自带）
# 建议使用 Python 3.8（JetPack 4.6支持）

# 安装Python 3.8
sudo apt install -y python3.8 python3.8-venv python3.8-dev

# 创建虚拟环境
python3.8 -m venv ~/projects/vision_system/venv
source ~/projects/vision_system/venv/bin/activate

# 升级pip
pip install --upgrade pip setuptools wheel
```

### 3.3 安装 PyTorch（关键步骤）

**Jetson Nano 必须使用 NVIDIA 专用 PyTorch 版本！**

```bash
# 方式1：下载NVIDIA预编译wheel（推荐，约1分钟）
# JetPack 4.6 对应 PyTorch 1.10.0 或 2.0.0

# PyTorch 1.10.0（稳定版）
wget https://developer.download.nvidia.com/compute/redist/jp/v461/pytorch/torch-1.10.0+nv21.12-cp38-cp38-linux_aarch64.whl
pip install torch-1.10.0+nv21.12-cp38-cp38-linux_aarch64.whl

# PyTorch 2.0.0（新版）
wget https://developer.download.nvidia.com/compute/redist/jp/v461/pytorch/torch-2.0.0+nv23.05-cp38-cp38-linux_aarch64.whl
pip install torch-2.0.0+nv23.05-cp38-cp38-linux_aarch64.whl

# 安装torchvision（需从源码编译）
pip install torchvision==0.11.1  # 对应torch 1.10
# 或
pip install torchvision==0.15.1  # 对应torch 2.0

# 验证安装
python -c "import torch; print(f'PyTorch: {torch.__version__}'); print(f'CUDA可用: {torch.cuda.is_available()}')"
# 期望输出：
# PyTorch: 1.10.0+nv21.12
# CUDA可用: True
```

### 3.4 安装其他依赖

```bash
# 修改后的requirements.txt（Jetson专用）
pip install \
    opencv-python-headless==4.5.4.60 \
    numpy==1.21.6 \
    pyserial==3.5 \
    Pillow==9.0.1 \
    qrcode==7.3.1 \
    ultralytics==8.0.196

# 注意：
# - opencv-python-headless：Jetson无GUI，必须用headless版
# - ultralytics版本锁定：新版可能不兼容ARM架构
```

### 3.5 安装 OpenCV（可选系统版本）

```bash
# 方式1：pip安装（简单，但性能较低）
pip install opencv-python-headless

# 方式2：系统OpenCV（性能更好，支持硬件加速）
sudo apt install -y libopencv-dev python3-opencv

# 方式3：从源码编译（最优性能，耗时约30分钟）
# 参考：https://docs.opencv.org/4.x/d6/d15/tutorial_building_tegra.html
```

---

## 4. 配置修改

### 4.1 修改 config.py

```python
# config.py - Jetson Nano 专用配置

# ========== 串口通信配置 ==========
# 查看实际串口设备：ls /dev/ttyUSB* 或 ls /dev/ttyACM*
SERIAL_PORT = '/dev/ttyUSB0'  # 修改为Jetson实际设备路径
SERIAL_BAUDRATE = 115200
SERIAL_TIMEOUT = 0.05
SERIAL_MOCK = False           # 实际硬件连接时改为False
SERIAL_MOCK_CYCLE = False

# ========== 摄像头配置 ==========
# Jetson摄像头索引需用 v4l2-ctl --list-devices 检测
CAMERA_MAIN_INDEX = 0         # 修改为实际索引（通常是0）
CAMERA_QR_INDEX = 1           # 如果只有一个摄像头，改为0或注释掉二维码功能
CAMERA_MAIN_WIDTH = 640
CAMERA_MAIN_HEIGHT = 480
CAMERA_QR_WIDTH = 640
CAMERA_QR_HEIGHT = 480
```

### 4.2 修改 yolo_dataset/data.yaml

```yaml
# yolo_dataset/data.yaml
path: /home/你的用户名/projects/vision_system/yolo_dataset  # 修改为Jetson绝对路径
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

### 4.3 环境变量设置

```bash
# 添加到 ~/.bashrc
export CUDA_HOME=/usr/local/cuda-10.2
export PATH=$CUDA_HOME/bin:$PATH
export LD_LIBRARY_PATH=$CUDA_HOME/lib64:$LD_LIBRARY_PATH

# 激活虚拟环境
source ~/projects/vision_system/venv/bin/activate

# 设置Python默认版本（可选）
alias python=python3.8
alias pip=pip3.8
```

---

## 5. 代码修改

### 5.1 摄像头管理器修改

**文件**：`vision/camera_manager.py`

```python
# 在 CameraManager 类的 open() 方法中添加

def open(self):
    """打开摄像头"""
    # Jetson必须指定V4L2后端
    backend = cv2.CAP_V4L2 if os.name != 'nt' else cv2.CAP_DSHOW

    self.main_cap = cv2.VideoCapture(self.main_index, backend)
    self.main_cap.set(cv2.CAP_PROP_FRAME_WIDTH, self.main_width)
    self.main_cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.main_height)

    # Jetson曝光设置（正值，Windows用负值）
    if os.name != 'nt':
        # Linux/Jetson: 手动曝光模式
        self.main_cap.set(cv2.CAP_PROP_AUTO_EXPOSURE, 1)
        self.main_cap.set(cv2.CAP_PROP_EXPOSURE, 120)  # 正值
    else:
        # Windows: 手动曝光
        self.main_cap.set(cv2.CAP_PROP_AUTO_EXPOSURE, 0.25)
        self.main_cap.set(cv2.CAP_PROP_EXPOSURE, -6)   # 负值

    # 二维码摄像头（如有）
    if self.qr_index >= 0:
        self.qr_cap = cv2.VideoCapture(self.qr_index, backend)
        self.qr_cap.set(cv2.CAP_PROP_FRAME_WIDTH, self.qr_width)
        self.qr_cap.set(cv2.CAP_PROP_FRAME_HEIGHT, self.qr_height)
```

### 5.2 YOLO推理参数调整

**文件**：`vision/yolo_detector.py`

```python
# Jetson Nano 性能优化参数

class YOLOv8Detector:
    def __init__(self, model_path='yolov8n.pt', conf_threshold=0.5,
                 device=0, imgsz=640):
        # Jetson Nano 推理尺寸建议降至320或416（提升FPS）
        self.imgsz = 320 if self._is_jetson() else imgsz

        # Jetson 内存有限，降低batch处理
        self.batch_size = 1

    def _is_jetson(self):
        """检测是否在Jetson平台"""
        try:
            with open('/etc/nv_tegra_release', 'r') as f:
                return True
        except FileNotFoundError:
            return False
```

### 5.3 显示模式处理

**文件**：`vision_system.py`

```python
# Jetson无GUI环境处理

def main():
    # 检测是否支持GUI显示
    gui_available = 'DISPLAY' in os.environ or check_gui_available()

    if not gui_available:
        print("[Jetson] headless模式，关闭GUI显示")
        # 只保存检测结果图片，不imshow
        save_mode = True
    else:
        # 远程桌面/VNC环境下可显示
        save_mode = False

    # ... 主循环中
    if gui_available:
        cv2.imshow('Vision System', processed_img)
    else:
        # Jetson headless：保存关键帧
        if frame_count % 30 == 0:  # 每秒保存1帧
            cv2.imwrite(f'output/frame_{frame_count}.jpg', processed_img)
```

---

## 6. 硬件检测与验证

### 6.1 检测摄像头

```bash
# 列出所有摄像头设备
v4l2-ctl --list-devices

# 输出示例：
# USB Camera (usb-3.0):
#   /dev/video0
#   /dev/video1

# 查看摄像头详细信息
v4l2-ctl -d /dev/video0 --info

# 测试摄像头采集
gst-launch-1.0 v4l2src device=/dev/video0 ! video/x-raw,width=640,height=480 ! videoconvert ! autovideosink
```

### 6.2 检测串口设备

```bash
# 列出USB串口设备
ls /dev/ttyUSB*
ls /dev/ttyACM*

# 查看设备详细信息
udevadm info -a -n /dev/ttyUSB0 | grep -E 'idVendor|idProduct'

# 测试串口通信（安装minicom）
sudo apt install minicom
sudo minicom -s
# 设置串口设备路径和波特率
```

### 6.3 GPU性能测试

```bash
# 检查GPU状态
tegrastats
# 输出：RAM 2048/3964MB, GPU 0%@767MHz

# GPU频率设置（可选，提升性能）
sudo jetson_clocks  # 开启最大性能模式

# YOLO推理速度测试
cd ~/projects/vision_system
python -c "
from ultralytics import YOLO
model = YOLO('yolo_pipeline/runs/detect/runs/material_detection-4/weights/best.pt')
results = model.predict(source='yolo_dataset/images/train', imgsz=320, device=0)
print('推理完成')
"
```

---

## 7. 性能优化建议

### 7.1 内存优化

Jetson Nano 内存仅 4GB，需严格控制：

```python
# YOLO推理优化
model = YOLO('best.pt')
results = model.predict(
    source=frame,
    imgsz=320,       # 降低推理尺寸（640→320）
    half=True,       # FP16半精度推理（Jetson支持）
    device=0,
    verbose=False    # 关闭日志输出
)

# 及时释放内存
del results
torch.cuda.empty_cache()
```

### 7.2 FPS提升技巧

| 优化项 | 设置 | FPS提升 |
|--------|------|---------|
| 推理尺寸 | 640 → 320 | +50% |
| 半精度FP16 | half=True | +30% |
| 模型选择 | yolov8n（最小） | - |
| 批处理 | batch=1 | - |

**预期FPS**：
- yolov8n + imgsz=320 + FP16：**15-20 FPS**
- yolov8n + imgsz=640：**8-10 FPS**

### 7.3 开启最大性能模式

```bash
# Jetson性能模式
sudo jetson_clocks  # 最大性能（功耗增加）

# 查看当前状态
sudo jetson_clocks --show

# GPU频率最大化
sudo jetson_clocks --gpmode max

# 恢复默认
sudo jetson_clocks --restore
```

---

## 8. 迁移验证清单

### 功能验证

```bash
cd ~/projects/vision_system

# 1. 激活环境
source venv/bin/activate

# 2. 测试摄像头
python tests/collect_materials.py  # 采集测试

# 3. 测试YOLO推理
python yolo_pipeline/inference.py --model yolo_pipeline/runs/detect/runs/material_detection-4/weights/best.pt --source 0

# 4. 测试串口通信
python -c "
from vision.serial_comm import SerialComm
ser = SerialComm(mock=False, port='/dev/ttyUSB0', baudrate=115200)
ser.open()
print('串口已打开')
"

# 5. 启动完整系统
python vision_system.py
```

---

## 9. 常见问题与解决

| 问题 | 原因 | 解决方法 |
|------|------|---------|
| `torch.cuda.is_available() = False` | PyTorch版本不对 | 安装NVIDIA专用PyTorch wheel |
| `ImportError: libopencv_core.so` | OpenCV未安装 | `pip install opencv-python-headless` |
| 摄像头打开失败 | V4L2后端未指定 | 代码中添加 `cv2.CAP_V4L2` |
| 串口Permission denied | 用户无dialout权限 | `sudo usermod -aG dialout $USER` |
| FPS过低（<5） | imgsz过大/未用FP16 | 降低imgsz至320，启用half=True |
| 内存溢出OOM | 模型过大 | 使用yolov8n，及时释放GPU内存 |

---

## 10. 迁移完成标志

```bash
# 最终验证脚本
python -c "
import torch
import cv2
from ultralytics import YOLO

print('='*50)
print('Jetson Nano 迁移验证')
print('='*50)
print(f'PyTorch: {torch.__version__}')
print(f'CUDA可用: {torch.cuda.is_available()}')
print(f'OpenCV: {cv2.__version__}')
print(f'GPU型号: {torch.cuda.get_device_name(0)}')

# 测试YOLO推理
model = YOLO('yolo_pipeline/runs/detect/runs/material_detection-4/weights/best.pt')
print(f'YOLO模型加载成功')
print('='*50)
print('迁移验证通过！')
"
```