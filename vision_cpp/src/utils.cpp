/**
 * 视觉系统工具模块实现
 * 对应 Python: vision/utils.py
 */
#include "utils.hpp"
#include <cstdlib>
#include <fstream>

FPSCounter::FPSCounter(int updateInterval)
    : m_updateInterval(updateInterval), m_fps(0.0), m_frameCount(0),
      m_startTime(std::chrono::steady_clock::now()) {}

double FPSCounter::tick() {
    m_frameCount++;
    if (m_frameCount >= m_updateInterval) {
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - m_startTime).count();
        m_fps = elapsed > 0 ? m_frameCount / elapsed : 0.0;
        m_frameCount = 0;
        m_startTime = now;
    }
    return m_fps;
}

bool checkGuiAvailable() {
    try {
        cv::namedWindow("test", cv::WINDOW_NORMAL);
        cv::destroyWindow("test");
        return true;
    } catch (...) {
        return false;
    }
}

cv::Mat generateTestFrame() {
    cv::Mat img(480, 640, CV_8UC3, cv::Scalar(50, 50, 50));
    cv::circle(img, cv::Point(200, 200), 30, cv::Scalar(0, 0, 255), -1);
    cv::circle(img, cv::Point(320, 200), 30, cv::Scalar(0, 255, 0), -1);
    cv::circle(img, cv::Point(440, 200), 30, cv::Scalar(255, 0, 0), -1);
    return img;
}

bool isJetson() {
    std::ifstream f("/etc/nv_tegra_release");
    return f.good();
}

bool isHeadless() {
    const char* display = std::getenv("DISPLAY");
    if (display == nullptr) return true;
    if (!checkGuiAvailable()) return true;
    return false;
}
