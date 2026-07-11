#!/bin/bash
# ============================================================
# Jetson Nano 视觉系统启动脚本
# 功能：自动激活虚拟环境 + 启动视觉系统
# ============================================================

# 获取脚本所在目录（项目根目录）
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$SCRIPT_DIR"

# 虚拟环境路径
VENV_DIR="$SCRIPT_DIR/venv"

# ========== 1. 检查虚拟环境 ==========
if [ ! -d "$VENV_DIR" ]; then
    echo "错误：虚拟环境不存在！"
    echo "请先运行: ./setup_jetson.sh"
    exit 1
fi

# ========== 2. 激活虚拟环境 ==========
echo "激活虚拟环境..."
source "$VENV_DIR/bin/activate"

# ========== 3. 检查依赖 ==========
echo "检查依赖..."
python -c "import torch; import cv2; import ultralytics" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "警告：部分依赖缺失，请检查安装"
fi

# ========== 4. 检查模型文件 ==========
MODEL_PATH="yolo_pipeline/runs/detect/runs/material_detection-4/weights/best.pt"
if [ ! -f "$MODEL_PATH" ]; then
    echo "警告：模型文件不存在: $MODEL_PATH"
    echo "请先训练模型或检查路径"
fi

# ========== 5. 检查摄像头 ==========
echo "检测摄像头设备..."
CAMERA_COUNT=$(ls /dev/video* 2>/dev/null | wc -l)
if [ "$CAMERA_COUNT" -eq 0 ]; then
    echo "警告：未检测到摄像头设备"
else
    echo "检测到 $CAMERA_COUNT 个摄像头设备"
fi

# ========== 6. 检查串口 ==========
echo "检测串口设备..."
SERIAL_COUNT=$(ls /dev/ttyUSB* /dev/ttyACM* 2>/dev/null | wc -l)
if [ "$SERIAL_COUNT" -eq 0 ]; then
    echo "提示：未检测到串口设备，将使用模拟模式"
else
    echo "检测到 $SERIAL_COUNT 个串口设备"
fi

# ========== 7. 开启性能模式（可选） ==========
if [ "$1" == "--performance" ]; then
    echo "开启 Jetson 最大性能模式..."
    sudo jetson_clocks
fi

# ========== 8. 启动视觉系统 ==========
echo ""
echo "============================================"
echo "  启动视觉系统"
echo "============================================"
echo "项目目录: $SCRIPT_DIR"
echo "Python: $(which python)"
echo "PyTorch: $(python -c 'import torch; print(torch.__version__)')"
echo "CUDA: $(python -c 'import torch; print(torch.cuda.is_available())')"
echo "============================================"
echo ""

# 根据参数选择启动模式
if [ "$1" == "--test" ]; then
    echo "运行测试模式..."
    python tests/test_jetson.py
elif [ "$1" == "--train" ]; then
    echo "运行训练模式..."
    cd yolo_pipeline
    python train_model.py --device 0
elif [ "$1" == "--config" ]; then
    echo "运行配置检测..."
    python config_jetson.py --apply
else
    echo "运行视觉主系统..."
    python vision_system.py
fi

# ========== 9. 退出处理 ==========
deactivate 2>/dev/null
echo ""
echo "系统已退出"