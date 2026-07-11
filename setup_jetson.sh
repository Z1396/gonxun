#!/bin/bash
# ============================================================
# Jetson Nano B01 一键部署脚本
# 项目：工创赛2025智能物流搬运视觉系统
# 目标：Jetson Nano B01 + Ubuntu 18.04 + JetPack 4.6
# ============================================================
set -e

echo "============================================"
echo "  Jetson Nano 视觉系统部署脚本"
echo "============================================"

# ========== 1. 检查系统 ==========
echo "[1/7] 检查系统环境..."
if [ ! -f /etc/nv_tegra_release ]; then
    echo "错误：未检测到 Jetson 设备！"
    exit 1
fi

echo "系统信息："
cat /etc/nv_tegra_release
echo ""

# ========== 2. 安装系统依赖 ==========
echo "[2/7] 安装系统依赖..."
sudo apt update
# 分开安装，避免依赖冲突
sudo apt install -y build-essential cmake pkg-config || true
sudo apt install -y libjpeg-dev libpng-dev || true
sudo apt install -y udev usbutils || true
sudo apt install -y python3-dev python3-pip || true
sudo apt install -y htop || true

# 尝试安装 v4l-utils（摄像头工具）
sudo apt install -y v4l-utils || echo "v4l-utils 安装失败，继续..."

# ========== 3. 创建虚拟环境 ==========
echo "[3/7] 创建Python虚拟环境..."
cd "$(dirname "$0")"
PROJECT_DIR="$(pwd)"
python3 -m venv venv
source venv/bin/activate
pip install --upgrade pip setuptools wheel

# ========== 4. 安装 PyTorch (Jetson专用) ==========
echo "[4/7] 安装 PyTorch (Jetson专用版本)..."
# JetPack 4.6 对应 PyTorch 1.10.0
PYTORCH_WHL="torch-1.10.0+nv21.12-cp38-cp38-linux_aarch64.whl"

if [ ! -f "$PYTORCH_WHL" ]; then
    echo "下载 PyTorch Jetson wheel..."
    wget https://developer.download.nvidia.com/compute/redist/jp/v461/pytorch/$PYTORCH_WHL
fi

pip install "$PYTORCH_WHL"
pip install torchvision==0.11.1

# 验证 PyTorch
python -c "import torch; print(f'PyTorch: {torch.__version__}'); print(f'CUDA: {torch.cuda.is_available()}')"

# ========== 5. 安装项目依赖 ==========
echo "[5/7] 安装项目依赖..."
# Jetson 专用 requirements
pip install \
    opencv-python-headless==4.5.4.60 \
    numpy==1.21.6 \
    pyserial==3.5 \
    Pillow==9.0.1 \
    qrcode==7.3.1 \
    ultralytics==8.0.196

# ========== 6. 配置硬件权限 ==========
echo "[6/7] 配置硬件权限..."

# 串口权限（CH341）
echo 'SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", MODE="0666", SYMLINK+="ttyCH341USB0"' | \
    sudo tee /etc/udev/rules.d/99-ch341.rules
sudo udevadm control --reload-rules
sudo udevadm trigger

# 用户组权限
sudo usermod -aG dialout $USER
sudo usermod -aG video $USER
sudo usermod -aG tty $USER

# ========== 7. 检测硬件 ==========
echo "[7/7] 检测硬件设备..."

echo ""
echo "摄像头设备："
v4l2-ctl --list-devices 2>/dev/null || echo "未检测到摄像头"

echo ""
echo "串口设备："
ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null || echo "未检测到USB串口设备"

echo ""
echo "============================================"
echo "  部署完成!"
echo "============================================"
echo ""
echo "后续步骤:"
echo "  1. 重新登录以应用用户组权限"
echo "  2. 修改 config.py 配置："
echo "     - CAMERA_MAIN_INDEX（用 v4l2-ctl --list-devices 检测）"
echo "     - SERIAL_PORT（用 ls /dev/ttyUSB* 检测）"
echo "     - SERIAL_MOCK = False（连接真实硬件）"
echo "  3. 修改 yolo_dataset/data.yaml 路径为 Jetson 绝对路径"
echo "  4. 激活环境: source venv/bin/activate"
echo "  5. 运行系统: python vision_system.py"
echo ""
echo "性能优化（可选）："
echo "  - 开启最大性能模式: sudo jetson_clocks"
echo "  - 监控GPU状态: tegrastats"
echo ""
echo "项目目录: $PROJECT_DIR"