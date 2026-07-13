/**
 * 卡尔曼滤波器模块
 * 对应 Python: vision/kalman_filter.py
 * 二维卡尔曼滤波，用于平滑图像识别得到的物体中心坐标
 */
#pragma once

#include <opencv2/core.hpp>

class KalmanFilter {
public:
    /**
     * @param q 过程噪声协方差 (越小越信任模型预测)
     * @param r 观测噪声协方差 (越小越信任测量值)
     */
    explicit KalmanFilter(double q = 1e-5, double r = 1e-2);

    /** 预测步骤 */
    void predict();

    /** 更新步骤：融合预测值与观测值 */
    cv::Matx21f update(const cv::Matx21f& z);

    /** 一步完成预测+更新，返回滤波后坐标 */
    cv::Matx21f filter(const cv::Matx21f& z);

    /** 重置滤波器状态 */
    void reset();

private:
    double m_q;
    double m_r;
    cv::Matx21f m_x;      // 状态量 [x, y]
    cv::Matx22f m_p;      // 协方差矩阵
    bool m_initialized;
};
