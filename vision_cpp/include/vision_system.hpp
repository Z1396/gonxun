/**
 * 视觉系统核心调度模块
 * 对应 Python: vision/system.py
 * VisionSystem 统一管理视觉、串口、相机、滤波模块
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
#include <opencv2/opencv.hpp>
#include <array>
#include <memory>
#include <string>

class VisionSystem {
public:
    VisionSystem(bool serialMock = true,
                 const std::string& serialPort = "",
                 int baudrate = 115200,
                 int mainCamera = -1,
                 int qrCamera = -1);
    ~VisionSystem() = default;

    /** 单帧图像统一处理入口 */
    cv::Mat processFrame(const cv::Mat& img, int unit = -1);

    // 公开成员（供 main 直接访问）
    SerialComm serialComm;
    CameraManager camera;
    QRDetector qrDetector;
    TaskCodeParser taskParser;

private:
    /** 卡尔曼滤波坐标 */
    std::pair<int, int> filterPosition(float x, float y, int kfIndex);

    /** 检测三种颜色并滤波、绘制 */
    std::vector<std::pair<int, int>> detectThreeColors(
        cv::Mat& img,
        const std::vector<std::tuple<std::string, std::string, cv::Scalar>>& colorSpecs,
        int minArea, int maxArea);

    // 各模式处理函数
    void processColor(cv::Mat& resultImg);
    void processRing(cv::Mat& resultImg);
    void processDock(cv::Mat& resultImg);
    void processQr(cv::Mat& resultImg);

    // 子模块实例
    YOLOv8Detector m_yoloDetector;
    ThreeRingDetector m_threeRingDetector;
    SixRingDetector m_sixRingDetector;
    ObstacleDetector m_obstacleDetector;
    TaskDisplay m_taskDisplay;

    // 3个卡尔曼滤波器
    std::array<KalmanFilter, 3> m_kalmanFilters;
};
