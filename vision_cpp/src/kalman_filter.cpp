/**
 * @file kalman_filter.cpp
 * @brief 二维卡尔曼滤波器实现
 *
 * 实现简化的 x/y 解耦卡尔曼滤波。状态方程和观测方程均为恒等模型，
 * x/y 各自独立执行 predict→update 循环。
 */
#include "kalman_filter.hpp"

/**
 * @brief 构造函数，初始化噪声参数和状态
 * @param q 过程噪声协方差
 * @param r 观测噪声协方差
 * @note 初始协方差 P 设为单位阵，初始状态设为原点
 */
KalmanFilter::KalmanFilter(double q, double r)
    : q_(q),
      r_(r),
      x_(0.f, 0.f),
      p_(1.f, 0.f, 0.f, 1.f),
      initialized_(false)
{}

/**
 * @brief 预测步骤，更新先验协方差
 * @note P = P + Q（x/y 各自独立加 q_）
 */
void KalmanFilter::predict() {
    p_(0, 0) += q_;
    p_(1, 1) += q_;
}

/**
 * @brief 更新步骤，融合观测值
 * @param z 观测向量 [zx, zy]
 * @return 滤波后的状态估计 [x, y]
 * @note 计算卡尔曼增益 K = P/(P+R)，更新状态 x = x + K*(z-x)，
 *       更新协方差 P = (1-K)*P。x/y 独立计算。
 */
cv::Matx21f KalmanFilter::update(const cv::Matx21f& z) {
    float p00 = p_(0, 0);
    float p11 = p_(1, 1);

    // P + R
    float pr0 = p00 + static_cast<float>(r_);
    float pr1 = p11 + static_cast<float>(r_);

    // 卡尔曼增益 K = P / (P + R)
    float k0 = pr0 / (pr0 + static_cast<float>(r_));
    float k1 = pr1 / (pr1 + static_cast<float>(r_));

    // 新息 (innovation) = z - x
    float innov0 = z(0) - x_(0);
    float innov1 = z(1) - x_(1);

    // 状态更新: x = x + K * innovation
    x_(0) += k0 * innov0;
    x_(1) += k1 * innov1;

    // 协方差更新: P = (1 - K) * P
    p_(0, 0) = (1.f - k0) * p00;
    p_(1, 1) = (1.f - k1) * p11;

    return x_;
}

/**
 * @brief 预测+更新一步完成
 * @param z 观测向量 [zx, zy]
 * @return 滤波后的状态估计 [x, y]
 */
cv::Matx21f KalmanFilter::filter(const cv::Matx21f& z) {
    predict();
    auto result = update(z);
    initialized_ = true;
    return result;
}

/** @brief 重置滤波器状态和协方差到初始值 */
void KalmanFilter::reset() {
    x_ = cv::Matx21f(0.f, 0.f);
    p_ = cv::Matx22f(1.f, 0.f, 0.f, 1.f);
    initialized_ = false;
}
