#!/bin/bash
# ============================================================
# Ubuntu 18.04 一键部署脚本 - YOLO物块识别视觉系统
# 使用方式: chmod +x setup_ubuntu.sh && ./setup_ubuntu.sh
# ============================================================
set -e

echo "============================================"
echo "  YOLO视觉系统 - Ubuntu 18.04 部署脚本"
echo "============================================"

# ========== 1. 安装系统依赖 ==========
echo "[1/6] 安装系统依赖..."
sudo apt update
sudo apt install -y \
    software-properties-common \
    build-essential cmake pkg-config \
    libjpeg-dev libpng-dev libtiff-dev \
    libavcodec-dev libavformat-dev libswscale-dev \
    libv4l-dev libxvidcore-dev libx264-dev \
    libgtk-3-dev libcanberra-gtk-module \
    libfontconfig1 libxext6 \
    udev usbutils v4l-utils

# ========== 2. 安装Python 3.8 ==========
echo "[2/6] 安装Python 3.8..."
sudo add-apt-repository -y ppa:deadsnakes/ppa
sudo apt update
sudo apt install -y python3.8 python3.8-venv python3.8-dev

# ========== 3. 创建虚拟环境 ==========
echo "[3/6] 创建虚拟环境..."
python3.8 -m venv venv
source venv/bin/activate
pip install --upgrade pip

# ========== 4. 安装PyTorch ==========
echo "[4/6] 安装PyTorch (CPU版本)..."
pip install torch==2.0.1+cpu torchvision==0.15.2+cpu \
    -f https://download.pytorch.org/whl/torch_stable.html

# ========== 5. 安装项目依赖 ==========
echo "[5/6] 安装项目依赖..."
# 桌面版用 opencv-python（支持imshow），服务器版用 opencv-python-headless
if command -v xvfb-run &>/dev/null || [ -z "$DISPLAY" ]; then
    echo "检测到无显示环境，使用 opencv-python-headless"
    pip install opencv-python-headless
else
    echo "检测到桌面环境，使用 opencv-python"
    pip install opencv-python
fi
pip install numpy>=1.21.0 pyserial>=3.5 Pillow>=9.0.0 qrcode>=7.0.0 ultralytics>=8.0.0

# ========== 6. 配置硬件权限 ==========
echo "[6/6] 配置硬件权限..."

# 串口权限
echo 'SUBSYSTEM=="tty", ATTRS{idVendor}=="1a86", ATTRS{idProduct}=="7523", MODE="0666", SYMLINK+="ttyCH341USB0"' | \
    sudo tee /etc/udev/rules.d/99-ch341.rules
sudo udevadm control --reload-rules
sudo udevadm trigger

# 用户组权限
sudo usermod -aG dialout $USER
sudo usermod -aG video $USER

echo ""
echo "============================================"
echo "  部署完成!"
echo "============================================"
echo ""
echo "后续步骤:"
echo "  1. 重新登录以应用用户组权限"
echo "  2. 激活虚拟环境: source venv/bin/activate"
echo "  3. 检测摄像头: v4l2-ctl --list-devices"
echo "  4. 修改 config.py 中摄像头索引"
echo "  5. 运行: python vision_system.py"
echo ""
echo "GPU加速（可选）:"
echo "  1. 安装CUDA 11.x: https://developer.nvidia.com/cuda-11-8-0-download-archive"
echo "  2. 重装PyTorch GPU版:"
echo "     pip install torch==2.0.1+cu118 -f https://download.pytorch.org/whl/torch_stable.html"
echo ""
