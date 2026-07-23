/**
 * @file config.hpp
 * @brief 系统常量配置
 *
 * 集中定义视觉系统各模块的运行参数，包括串口通信、摄像头、
 * 颜色识别、卡尔曼滤波、仿真场地、YOLO模型等配置。
 * 所有常量使用 inline constexpr 以支持跨编译单元共享。
 */
#pragma once

#include "field_constants.hpp"
#include <cstdint>
#include <string>

namespace config {

// ==== 日志配置 ====
inline constexpr const char* LOG_LEVEL = "INFO";          ///< 日志级别
inline constexpr const char* LOG_FORMAT = "%(asctime)s [%(levelname)s] %(message)s"; ///< 日志格式

// ==== 串口通信配置 ====
inline const std::string SERIAL_PORT = "/dev/ttyCH341USB0"; ///< 串口设备路径 (CH341 USB转串口)
inline constexpr int SERIAL_BAUDRATE = 115200;              ///< 波特率
inline constexpr double SERIAL_TIMEOUT = 0.05;              ///< 读取超时 (秒)
inline constexpr bool SERIAL_MOCK = true;                   ///< 默认使用模拟串口
inline constexpr bool SERIAL_MOCK_CYCLE = true;             ///< 模拟模式下周期切换工作模式

// ==== 摄像头配置 ====
inline constexpr int CAMERA_MAIN_INDEX = 1;    ///< 主摄像头设备索引
inline constexpr int CAMERA_QR_INDEX = 2;      ///< 扫码摄像头设备索引
inline constexpr int CAMERA_MAIN_WIDTH = 640;  ///< 主摄像头分辨率宽度 (px)
inline constexpr int CAMERA_MAIN_HEIGHT = 480; ///< 主摄像头分辨率高度 (px)
inline constexpr int CAMERA_QR_WIDTH = 640;    ///< 扫码摄像头分辨率宽度 (px)
inline constexpr int CAMERA_QR_HEIGHT = 480;   ///< 扫码摄像头分辨率高度 (px)

// ==== 颜色识别配置 ====
inline constexpr int COLOR_MIN_AREA = 2000;    ///< 颜色轮廓最小面积阈值 (px²)
inline constexpr int COLOR_DOCK_MIN_AREA = 3000; ///< 对接模式颜色轮廓最小面积 (px²)

// ==== 卡尔曼滤波配置 ====
inline constexpr double KALMAN_Q = 1e-5;  ///< 过程噪声协方差，越小越信任预测
inline constexpr double KALMAN_R = 1e-2;  ///< 观测噪声协方差，越小越信任观测

// ==== 仿真场地配置 (单位: mm) ====
inline constexpr int FIELD_SIZE = gonxun::FIELD_SIZE_MM; ///< 场地边长 (mm)，来自 field_constants
inline constexpr double PIXEL_PER_MM = 0.22;  ///< 像素/毫米换算系数
inline constexpr int LANE_WIDTH = 400;         ///< 通道宽度 (mm)
inline constexpr int LANE_CENTER = 1200;       ///< 通道中心坐标 (mm)
inline constexpr int LANE_START = 1000;        ///< 通道起始坐标 (mm)
inline constexpr int LANE_END = 1400;          ///< 通道结束坐标 (mm)

// ==== YOLO 模型路径 ====
inline const std::string YOLO_MODEL_PATH =
    "yolo_pipeline/runs/detect/runs/material_detection-4/weights/best.pt"; ///< PyTorch 模型路径
inline const std::string YOLO_TS_MODEL_PATH =
    "yolo_pipeline/runs/detect/runs/material_detection-4/weights/best.torchscript"; ///< TorchScript 模型路径
inline constexpr double YOLO_CONF_THRESHOLD = 0.5;  ///< 检测置信度阈值
inline constexpr double YOLO_IOU_THRESHOLD = 0.45;  ///< NMS IoU 阈值
inline constexpr int YOLO_IMGSZ = 640;               ///< 推理输入图像尺寸 (px)
inline constexpr bool YOLO_HALF = false;             ///< 是否使用 FP16 半精度推理

} // namespace config
