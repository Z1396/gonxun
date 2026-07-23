/**
 * @file utils.hpp
 * @brief 视觉系统工具：FPS计数、GUI检测、测试帧生成、平台检测
 *
 * 提供运行时辅助功能，不依赖具体检测算法。
 * FPSCounter 用于性能监控；平台检测函数用于自适应配置。
 */
#pragma once

#include <chrono>
#include <string>
#include <opencv2/opencv.hpp>

/**
 * @brief FPS 计数器，按固定帧间隔更新统计值
 */
class FPSCounter {
public:
    /**
     * @brief 构造函数
     * @param update_interval 每 N 帧更新一次 FPS，默认 10
     */
    explicit FPSCounter(int update_interval = 10);

    /**
     * @brief 每帧调用，帧数达到间隔时重新计算 FPS
     * @return 当前 FPS 值
     */
    [[nodiscard]] double tick();

private:
    int update_interval_;                              ///< 更新间隔帧数
    double fps_;                                       ///< 当前 FPS
    int frame_count_;                                  ///< 累计帧数
    std::chrono::steady_clock::time_point start_time_; ///< 计时起点
};

/**
 * @brief 检测 GUI 是否可用（能否创建 OpenCV 窗口）
 * @return 可用返回 true
 */
[[nodiscard]] bool check_gui_available();

/**
 * @brief 生成测试帧，包含三个彩色圆形（红/绿/蓝），用于无摄像头时调试
 * @return 640×480 BGR 测试图像
 */
[[nodiscard]] cv::Mat generate_test_frame();

/**
 * @brief 检测当前是否运行在 Jetson 平台
 * @return 是 Jetson 返回 true（通过 /etc/nv_tegra_release 判断）
 */
[[nodiscard]] bool is_jetson();

/**
 * @brief 检测当前是否为无头环境（无 DISPLAY 或 GUI 不可用）
 * @return 无头返回 true
 */
[[nodiscard]] bool is_headless();
