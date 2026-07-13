/**
 * 卡尔曼滤波器模块实现
 * 对应 Python: vision/kalman_filter.py
 */
#include "kalman_filter.hpp"

KalmanFilter::KalmanFilter(double q, double r)
    : m_q(q), m_r(r), m_x(0.f, 0.f), m_p(1.f, 0.f, 0.f, 1.f), m_initialized(false) {}

void KalmanFilter::predict() {
    // P = P + Q (对角矩阵加法)
    m_p(0, 0) += m_q;
    m_p(1, 1) += m_q;
}

cv::Matx21f KalmanFilter::update(const cv::Matx21f& z) {
    // 2x2 对角矩阵求逆优化：闭式解
    float p00 = m_p(0, 0);
    float p11 = m_p(1, 1);
    float pr0 = p00 + m_r;
    float pr1 = p11 + m_r;
    float k0 = pr0 / (pr0 + static_cast<float>(m_r));
    float k1 = pr1 / (pr1 + static_cast<float>(m_r));

    // innovation = z - x
    float innov0 = z(0) - m_x(0);
    float innov1 = z(1) - m_x(1);

    // x = x + K * innovation
    m_x(0) += k0 * innov0;
    m_x(1) += k1 * innov1;

    // P = (I - K) * P
    m_p(0, 0) = (1.f - k0) * p00;
    m_p(1, 1) = (1.f - k1) * p11;

    return m_x;
}

cv::Matx21f KalmanFilter::filter(const cv::Matx21f& z) {
    predict();
    auto result = update(z);
    m_initialized = true;
    return result;
}

void KalmanFilter::reset() {
    m_x = cv::Matx21f(0.f, 0.f);
    m_p = cv::Matx22f(1.f, 0.f, 0.f, 1.f);
    m_initialized = false;
}
