/**
 * 视觉系统工具模块
 * 对应 Python: vision/utils.py
 * - FPSCounter: 帧率计数器
 * - checkGuiAvailable: 检测 OpenCV 图形窗口支持
 * - generateTestFrame: 生成测试画布
 */
#pragma once

#include <opencv2/opencv.hpp>
#include <chrono>
#include <string>

/** FPS 帧率计数器 */
class FPSCounter {
public:
    explicit FPSCounter(int updateInterval = 10);

    /** 每帧调用，返回当前 FPS */
    double tick();

private:
    int m_updateInterval;
    double m_fps;
    int m_frameCount;
    std::chrono::steady_clock::time_point m_startTime;
};

/** 检测当前环境是否支持 OpenCV 图形窗口 */
bool checkGuiAvailable();

/** 生成测试画布（相机读取失败时使用） */
cv::Mat generateTestFrame();

/** 检测是否为 Jetson 平台 */
bool isJetson();

/** 检测是否为 headless 模式（无显示器） */
bool isHeadless();
