/**
 * @file vision_system.hpp
 * @brief 视觉系统核心调度模块
 *
 * 根据下位机下发的工作模式（COLOR/RING/DOCK/QR）分发
 * 到对应的处理流程，并负责卡尔曼滤波坐标平滑和串口结果回传。
 * 是整个视觉处理流水线的顶层协调者。
 */
#pragma once

#include "yolo_detector.hpp"
#include "ring_detector.hpp"
#include "qr_detector.hpp"
#include "kalman_filter.hpp"
#include "serial_comm.hpp"
#include "camera_manager.hpp"
#include "task_display.hpp"
#include "obstacle_detector.hpp"
#include "config_loader.hpp"
#include <array>
#include <atomic>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <opencv2/opencv.hpp>

// ==== 视觉检测模式常量（VisionSystem 内部用，与下位机协议无关） ====

constexpr uint8_t VISION_IDLE  = 0;   ///< 空闲
constexpr uint8_t VISION_COLOR = 1;   ///< 物料颜色识别
constexpr uint8_t VISION_RING  = 3;   ///< 圆环检测
constexpr uint8_t VISION_DOCK  = 4;   ///< 对接停靠
constexpr uint8_t VISION_QR    = 9;   ///< 二维码扫描

/// @brief 颜色编号到名称的映射 (1=红, 2=黄, 3=蓝, 4=绿, 5=黑, 6=浅蓝)
[[nodiscard]] inline std::string color_code_to_name(int code) {
    switch (code) {
        case 1: return "red";
        case 2: return "yellow";
        case 3: return "blue";
        case 4: return "green";
        case 5: return "black";
        case 6: return "light_blue";
        default: return "";
    }
}

/// @brief 颜色编号到BGR绘制颜色的映射
[[nodiscard]] inline cv::Scalar color_code_to_bgr(int code) {
    switch (code) {
        case 1: return {0, 0, 255};       // red
        case 2: return {0, 255, 255};     // yellow
        case 3: return {255, 0, 0};       // blue
        case 4: return {0, 255, 0};       // green
        case 5: return {0, 0, 0};         // black
        case 6: return {255, 255, 0};     // light_blue (cyan)
        default: return {128, 128, 128};  // gray
    }
}

/// @brief 颜色编号到显示标签的映射
[[nodiscard]] inline std::string color_code_to_label(int code) 
{
    switch (code) 
    {
        case 1: return "R";
        case 2: return "Y";
        case 3: return "B";
        case 4: return "G";
        case 5: return "K";
        case 6: return "C";
        default: return "?";
    }
}

/**
 * @brief 视觉系统核心调度类
 *
 * 整合 YOLO 检测器、圆环检测器、QR 检测器、障碍物检测器、
 * 卡尔曼滤波器和串口通信，按工作模式执行对应处理流程。
 */
class VisionSystem {
public:
    /**
     * @brief 构造函数：从全局配置初始化全部子模块
     * @param cfg 系统配置（串口、摄像头、YOLO 等参数均从中读取）
     * @param serial_comm 外部传入的串口通信实例（由 main 统一管理，与 MainWindow 共享）
     */
    explicit VisionSystem(const gonxun::Config& cfg, gonxun::SerialComm& serial_comm);
    ~VisionSystem() = default;

    /**
     * @brief 单帧图像统一处理入口
     * @param img 输入 BGR 图像
     * @param unit 工作模式，-1 则读取当前视觉模式
     * @return 标注后的结果图像
     */
    [[nodiscard]] cv::Mat process_frame(const cv::Mat& img, int unit = -1);

    /**
     * @brief 设置当前任务码
     * @param task_code 任务码结构
     */
    void set_task_code(const TaskCode& task_code);

    /**
     * @brief 设置当前批次 (1 或 2)
     * @param batch 批次号
     */
    void set_current_batch(int batch);

    gonxun::SerialComm& serial_comm;          ///< 串口通信实例（外部注入，与 MainWindow 共享）
    CameraManager camera;             ///< 双摄像头管理器
    QRDetector qr_detector;           ///< QR 码检测器
    TaskCodeParser task_parser;        ///< 任务码解析器

    // ---- 视觉模式控制（替代原 serial_comm.unit 原子量） ----
    /// @brief 设置视觉检测模式（VISION_COLOR/RING/DOCK/QR）
    void set_vision_mode(uint8_t mode) noexcept {
        override_unit_.store(mode, std::memory_order_relaxed);
        manual_mode_.store(true, std::memory_order_relaxed);
    }
    /// @brief 读取当前视觉检测模式
    [[nodiscard]] uint8_t current_vision_mode() const noexcept {
        return override_unit_.load(std::memory_order_relaxed);
    }

    // ---- 物料坐标缓存（供上层在 mode=2 定位时取用） ----
    /// @brief 获取最近一次物料检测坐标列表
    [[nodiscard]] const std::vector<std::pair<uint16_t, uint16_t>>& material_coords() const noexcept {
        return last_material_coords_;
    }
    /// @brief 清空物料坐标缓存
    void clear_material_coords() noexcept { last_material_coords_.clear(); }

    // ---- 模式覆写机制（保留调试用，已封装在 set_vision_mode/current_vision_mode） ----
    std::atomic<bool> manual_mode_{false};       ///< 是否手动模式覆写
    std::atomic<uint8_t> override_unit_{VISION_IDLE}; ///< 手动模式下的视觉模式

private:
    /**
     * @brief 卡尔曼滤波平滑坐标
     * @param x 原始 x 坐标
     * @param y 原始 y 坐标
     * @param kf_index 滤波器索引 (0-2)
     * @return 滤波后的 (x, y) 坐标
     */
    [[nodiscard]] std::pair<int, int> filter_position(float x, float y, int kf_index);

    /**
     * @brief 检测三种颜色的目标中心并滤波
     * @param img 输入图像（同时绘制检测结果）
     * @param color_specs 颜色规格列表: (颜色名, 标签, 绘制颜色)
     * @param min_area 最小面积阈值
     * @param max_area 最大面积阈值
     * @return 滤波后的坐标列表；任一颜色未检测到返回空
     */
    [[nodiscard]] std::vector<std::pair<int, int>> detect_three_colors(
        cv::Mat& img,
        const std::vector<std::tuple<std::string, std::string, cv::Scalar>>& color_specs,
        int min_area, int max_area);

    /** @brief 颜色识别处理流程 (MODE_COLOR): 根据任务码检测对应颜色 → 滤波 → 串口发送 */
    void process_color(cv::Mat& result_img);
    /** @brief 圆环检测处理流程 (MODE_RING): 检测3环 → 滤波 → 串口发送 */
    void process_ring(cv::Mat& result_img);
    /** @brief 对接停靠处理流程 (MODE_DOCK): 检测蓝/绿/红 → 滤波 → 串口发送 */
    void process_dock(cv::Mat& result_img);
    /** @brief 二维码处理流程 (MODE_QR): 读取扫码摄像头 → 解码 → 串口发送 */
    void process_qr(cv::Mat& result_img);

    YOLOv8Detector yolo_detector_;                ///< YOLO 检测器
    ThreeRingDetector three_ring_detector_;       ///< 3色环检测器
    SixRingDetector six_ring_detector_;           ///< 6环检测器
    ObstacleDetector obstacle_detector_;          ///< 障碍物检测器
    TaskDisplay task_display_;                    ///< 任务码显示渲染器

    std::array<KalmanFilter, 3> kalman_filters_;  ///< 3路卡尔曼滤波器（3个目标各自独立滤波）

    TaskCode current_task_;                       ///< 当前任务码
    int current_batch_{1};                         ///< 当前批次 (1 或 2)
    bool task_set_{false};                         ///< 任务码是否已设置

    std::vector<std::pair<uint16_t, uint16_t>> last_material_coords_;  ///< 最近一次物料检测坐标缓存

public:
    /// @brief QR码扫描回调函数类型
    using QRCallback = std::function<void(const std::string&)>;

    /// @brief 设置QR码扫描回调
    /// @param callback 扫码成功时调用的回调函数
    void set_qr_callback(QRCallback callback) { qr_callback_ = std::move(callback); }

private:
    QRCallback qr_callback_;                      ///< QR码扫描回调
};
