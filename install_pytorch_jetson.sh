#!/bin/bash
# Jetson Nano PyTorch GPU 版本安装脚本
# 针对 JetPack 4.6 (R32.7) + Python 3.6

echo "============================================"
echo "  Jetson Nano PyTorch GPU 安装"
echo "============================================"

# 安装依赖
echo "[1/3] 安装依赖..."
sudo apt install -y libopenblas-base libopenblas-dev

# 尝试多种安装方法
echo "[2/3] 安装 PyTorch..."

# 方法1：尝试从 pip 安装 GPU 版本
pip install torch torchvision --extra-index-url https://developer.download.nvidia.cn/compute/redist/jp/

# 如果方法1失败，尝试方法2：直接安装指定版本
if ! python -c "import torch" 2>/dev/null; then
    echo "方法1失败，尝试方法2..."
    pip install torch==1.9.0 torchvision==0.10.0
fi

# 如果方法2失败，尝试方法3：使用系统包
if ! python -c "import torch" 2>/dev/null; then
    echo "方法2失败，尝试方法3..."
    sudo apt install -y python3-pytorch
fi

# 测试安装
echo "[3/3] 测试安装..."
python -c "import torch; print(f'PyTorch: {torch.__version__}'); print(f'CUDA: {torch.cuda.is_available()}')"

echo "============================================"
echo "安装完成！"
echo "============================================"