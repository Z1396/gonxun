/**
 * 全局配置文件 (C++ 版本)
 * 对应 Python: config.py
 * 集中管理视觉系统所有可调参数
 */
#pragma once

#include <string>
#include <cstdint>

namespace config {

// ========== 日志配置 ==========
inline constexpr const char* LOG_LEVEL = "INFO";
inline constexpr const char* LOG_FORMAT = "%(asctime)s [%(levelname)s] %(message)s";

// ========== 串口通信配置 ==========
inline const std::string SERIAL_PORT = "/dev/ttyCH341USB0";
inline constexpr int SERIAL_BAUDRATE = 115200;
inline constexpr double SERIAL_TIMEOUT = 0.05;
inline constexpr bool SERIAL_MOCK = true;
inline constexpr bool SERIAL_MOCK_CYCLE = true;

// ========== 摄像头配置 ==========
inline constexpr int CAMERA_MAIN_INDEX = 1;
inline constexpr int CAMERA_QR_INDEX = 2;
inline constexpr int CAMERA_MAIN_WIDTH = 640;
inline constexpr int CAMERA_MAIN_HEIGHT = 480;
inline constexpr int CAMERA_QR_WIDTH = 640;
inline constexpr int CAMERA_QR_HEIGHT = 480;

// ========== 颜色识别配置 ==========
inline constexpr int COLOR_MIN_AREA = 2000;
inline constexpr int COLOR_DOCK_MIN_AREA = 3000;

// ========== 卡尔曼滤波配置 ==========
inline constexpr double KALMAN_Q = 1e-5;
inline constexpr double KALMAN_R = 1e-2;

// ========== 仿真场地配置 (单位: mm) ==========
inline constexpr int FIELD_SIZE = 2400;
inline constexpr double PIXEL_PER_MM = 0.22;
inline constexpr int LANE_WIDTH = 400;
inline constexpr int LANE_CENTER = 1200;
inline constexpr int LANE_START = 1000;
inline constexpr int LANE_END = 1400;

// ========== YOLO 模型路径 ==========
inline const std::string YOLO_MODEL_PATH =
    "yolo_pipeline/runs/detect/runs/material_detection-4/weights/best.pt";
inline const std::string YOLO_TS_MODEL_PATH =
    "yolo_pipeline/runs/detect/runs/material_detection-4/weights/best.torchscript";
inline constexpr double YOLO_CONF_THRESHOLD = 0.5;
inline constexpr double YOLO_IOU_THRESHOLD = 0.45;
inline constexpr int YOLO_IMGSZ = 640;
inline constexpr bool YOLO_HALF = false;

} // namespace config
