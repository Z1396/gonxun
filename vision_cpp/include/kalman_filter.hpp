/**
 * @file kalman_filter.hpp
 * @brief 二维卡尔曼滤波器，用于平滑图像识别坐标
 *
 * 实现简化的一维解耦卡尔曼滤波（x/y 独立滤波），
 * 用于消除颜色识别和圆环检测中的坐标抖动。
 * 状态向量 [x, y]，观测向量 [zx, zy]，
 * x/y 各自独立执行 predict→update 循环。
 */
#pragma once

#include <opencv2/core.hpp>

/**
 * @brief 二维卡尔曼滤波器（x/y 解耦）
 *
 * 状态方程: x_k = x_{k-1} + w,  w ~ N(0, Q)
 * 观测方程: z_k = x_k + v,      v ~ N(0, R)
 * Q 为过程噪声协方差，R 为观测噪声协方差。
 */
class KalmanFilter {
public:
    /**
     * @brief 构造函数
     * @param q 过程噪声协方差，越大越信任观测
     * @param r 观测噪声协方差，越大越信任预测
     */
    explicit KalmanFilter(double q = 1e-5, double r = 1e-2);

    /**
     * @brief 预测步骤，更新先验协方差
     * @note P = P + Q
     */
    void predict();

    /**
     * @brief 更新步骤，融合观测值
     * @param z 观测向量 [zx, zy]
     * @return 滤波后的状态估计 [x, y]
     * @note K = P/(P+R), x = x + K*(z-x), P = (1-K)*P
     */
    [[nodiscard]] cv::Matx21f update(const cv::Matx21f& z);

    /**
     * @brief 预测+更新一步完成
     * @param z 观测向量 [zx, zy]
     * @return 滤波后的状态估计 [x, y]
     */
    [[nodiscard]] cv::Matx21f filter(const cv::Matx21f& z);

    /** @brief 重置滤波器状态和协方差到初始值 */
    void reset();

private:
    double q_;             ///< 过程噪声协方差
    double r_;             ///< 观测噪声协方差
    cv::Matx21f x_;        ///< 状态估计 [x, y]
    cv::Matx22f p_;        ///< 协方差矩阵（对角近似）
    bool initialized_;     ///< 是否已初始化
};
