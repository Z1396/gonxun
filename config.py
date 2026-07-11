"""
全局配置文件
集中管理视觉系统与仿真系统的可调参数，避免硬编码分散在各模块中

平台配置说明：
- Windows: 使用当前默认配置
- Jetson Nano: 需修改以下配置项
"""

# ========== 日志配置 ==========
LOG_LEVEL = "INFO"           # DEBUG/INFO/WARNING/ERROR
LOG_FORMAT = "%(asctime)s [%(levelname)s] %(message)s"

# ========== 串口通信配置 ==========
# 【Jetson】修改为实际设备路径：ls /dev/ttyUSB* 或 ls /dev/ttyACM*
SERIAL_PORT = '/dev/ttyCH341USB0'  # Linux 默认 CH341 串口设备
# Jetson 常见路径：
# - USB转串口：'/dev/ttyUSB0'
# - Arduino/单片机：'/dev/ttyACM0'
# - CH341芯片：'/dev/ttyCH341USB0'

SERIAL_BAUDRATE = 115200
SERIAL_TIMEOUT = 0.05
SERIAL_MOCK = True                # 【Jetson】连接真实硬件时改为 False
SERIAL_MOCK_CYCLE = True          # 模拟模式是否循环切换 unit

# ========== 摄像头配置 ==========
# 【Jetson】使用 v4l2-ctl --list-devices 检测实际索引
CAMERA_MAIN_INDEX = 1  # 【Jetson】通常是 0
CAMERA_QR_INDEX = 2    # 【Jetson】如只有一个摄像头，改为 0 或禁用二维码功能
CAMERA_MAIN_WIDTH = 640
CAMERA_MAIN_HEIGHT = 480
CAMERA_QR_WIDTH = 640
CAMERA_QR_HEIGHT = 480

# ========== 颜色识别配置 ==========
COLOR_MIN_AREA = 2000             # 色块最小面积阈值
COLOR_DOCK_MIN_AREA = 3000        # 码垛模式最小面积阈值

# ========== 卡尔曼滤波配置 ==========
KALMAN_Q = 1e-5                   # 过程噪声协方差
KALMAN_R = 1e-2                   # 观测噪声协方差

# ========== 仿真场地配置（单位：mm） ==========
FIELD_SIZE = 2400
PIXEL_PER_MM = 0.22
LANE_WIDTH = 400
LANE_CENTER = FIELD_SIZE // 2
LANE_START = LANE_CENTER - LANE_WIDTH // 2
LANE_END = LANE_CENTER + LANE_WIDTH // 2

# 启停区
START_ZONE_1 = (50, 50, 300, 300)
START_ZONE_2 = (2050, 50, 300, 300)

# 原料转盘
RAW_ZONE_CENTER = (1000, 1200)
RAW_ZONE_RADIUS = 150

# 加工/存放区
ROUGH_ZONE = (200, 1450, 580, 150)
TEMP_ZONE = (1620, 1450, 580, 150)

# 二维码板
QR_BOARD_POS = (2100, 1000)
