/**
 * @file utils.cpp
 * @brief 视觉系统工具实现
 *
 * 实现 FPS 计数器、GUI 可用性检测、测试帧生成和平台检测。
 */
#include "utils.hpp"

#include <cstdlib>
#include <fstream>

/**
 * @brief 构造函数
 * @param update_interval 每 N 帧更新一次 FPS
 */
FPSCounter::FPSCounter(int update_interval)
    : update_interval_(update_interval), fps_(0.0), frame_count_(0),
      start_time_(std::chrono::steady_clock::now()) {}

/**
 * @brief 每帧调用，帧数达到间隔时重新计算 FPS
 * @return 当前 FPS 值
 */
double FPSCounter::tick() {
    frame_count_++;
    if (frame_count_ >= update_interval_) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - start_time_).count();
        fps_ = elapsed > 0 ? frame_count_ / elapsed : 0.0;
        frame_count_ = 0;
        start_time_ = now;
    }
    return fps_;
}

/**
 * @brief 检测 GUI 是否可用
 * @return 可用返回 true
 * @note 通过尝试创建/销毁 OpenCV 窗口来判断
 */
bool check_gui_available() {
    try {
        cv::namedWindow("test", cv::WINDOW_NORMAL);
        cv::destroyWindow("test");
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

/**
 * @brief 生成测试帧
 * @return 640×480 BGR 图像，包含红/绿/蓝三个圆形
 */
cv::Mat generate_test_frame() {
    cv::Mat img(480, 640, CV_8UC3, cv::Scalar(50, 50, 50));
    cv::circle(img, cv::Point(200, 200), 30, cv::Scalar(0, 0, 255), -1);
    cv::circle(img, cv::Point(320, 200), 30, cv::Scalar(0, 255, 0), -1);
    cv::circle(img, cv::Point(440, 200), 30, cv::Scalar(255, 0, 0), -1);
    return img;
}

/**
 * @brief 检测当前是否为 Jetson 平台
 * @return Jetson 返回 true
 * @note 通过 /etc/nv_tegra_release 文件是否存在判断
 */
bool is_jetson() {
    std::ifstream f("/etc/nv_tegra_release");
    return f.good();
}

/**
 * @brief 检测当前是否为无头环境
 * @return 无 DISPLAY 环境变量或 GUI 不可用时返回 true
 */
bool is_headless() {
    const char* display = std::getenv("DISPLAY");
    if (display == nullptr) return true;
    if (!check_gui_available()) return true;
    return false;
}
