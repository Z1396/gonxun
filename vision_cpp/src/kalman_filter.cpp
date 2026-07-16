/**
 * 卡尔曼滤波器模块实现
 * 对应 Python: vision/kalman_filter.py
 * 用途：二维坐标(x,y)专用简化版卡尔曼滤波
 * 适用场景：视觉目标坐标平滑、抑制图像识别抖动、滤除噪声
 * 模型简化说明：
 * 1. 状态向量仅 [x, y]，无速度分量，静态模型（假设目标短时间位置不变）
 * 2. x、y 两个维度完全解耦，互不干扰，分开计算增益与协方差
 * 3. Q 过程噪声、R 观测噪声均为对角常数，大幅简化矩阵运算，提速
 */
#include "kalman_filter.hpp"

/**
 * @brief 构造函数，初始化卡尔曼滤波器
 * @param q 过程噪声协方差 Q 的对角值，越大代表模型预测越不可信
 * @param r 观测噪声协方差 R 的对角值，越大代表相机检测坐标越不可信
 */
KalmanFilter::KalmanFilter(double q, double r)
    : m_q(q),                     // 存储过程噪声系数Q
      m_r(r),                     // 存储观测噪声系数R
      m_x(0.f, 0.f),              // 状态向量 [x, y]，滤波输出坐标
      m_p(1.f, 0.f, 0.f, 1.f),    // 误差协方差矩阵P 2×2，初始置信度1
      m_initialized(false)        // 滤波器是否接收过有效观测标记
{}

/**
 * @brief 预测步骤（卡尔曼第一步：先根据模型推算下一时刻状态）
 * 模型假设：静止模型，无运动转移，状态x不变；仅更新协方差P
 * 公式标准形式：P = A * P * A^T + Q
 * 本项目简化：状态转移矩阵A为单位矩阵I，因此直接 P = P + Q
 */
void KalmanFilter::predict() {
    // x轴方向协方差叠加过程噪声Q
    m_p(0, 0) += m_q;
    // y轴方向协方差叠加过程噪声Q
    m_p(1, 1) += m_q;
    // 非对角元p01/p10始终为0，xy解耦，无需处理
}

/**
 * @brief 更新步骤（卡尔曼第二步：用观测值修正预测结果）
 * @param z 当前视觉检测到的观测坐标 [z_x, z_y]
 * @return 滤波平滑后的最优坐标 [x, y]
 * 优化点：xy完全独立，直接用对角矩阵闭式解，避免通用矩阵求逆，算力开销极低
 */
cv::Matx21f KalmanFilter::update(const cv::Matx21f& z) {
    // 取出当前协方差矩阵对角元素 Pxx、Pyy
    float p00 = m_p(0, 0);
    float p11 = m_p(1, 1);

    // 计算分母 P + R，观测噪声补偿
    float pr0 = p00 + static_cast<float>(m_r);
    float pr1 = p11 + static_cast<float>(m_r);

    // 卡尔曼增益 K：决定观测值和预测值各占多少权重
    // K越大，越相信相机检测结果；K越小，越相信上一帧预测位置
    float k0 = pr0 / (pr0 + static_cast<float>(m_r));
    float k1 = pr1 / (pr1 + static_cast<float>(m_r));

    // 计算残差（观测值 - 预测值），即本次检测和预测的偏差
    float innov0 = z(0) - m_x(0);
    float innov1 = z(1) - m_x(1);

    // 状态更新：最优估计 = 预测值 + 卡尔曼增益 × 残差
    m_x(0) += k0 * innov0;
    m_x(1) += k1 * innov1;

    // 更新误差协方差矩阵 P = (I - K) * P
    // 代表修正后，下一帧预测的不确定性降低
    m_p(0, 0) = (1.f - k0) * p00;
    m_p(1, 1) = (1.f - k1) * p11;

    // 返回平滑后的坐标
    return m_x;
}

/**
 * @brief 完整滤波入口函数，单次观测调用一次
 * @param z 视觉识别原始坐标观测值
 * @return 平滑滤波后的稳定坐标
 * 流程：先预测 → 再观测更新 → 标记已初始化
 */
cv::Matx21f KalmanFilter::filter(const cv::Matx21f& z) {
    // 第一步：预测阶段
    predict();
    // 第二步：观测更新，得到平滑坐标
    auto result = update(z);
    // 标记滤波器已收到有效数据，不再是初始零状态
    m_initialized = true;
    return result;
}

/**
 * @brief 重置滤波器状态
 * 场景：目标丢失、切换目标、重新识别时调用
 * 清空坐标、重置协方差、恢复未初始化状态
 */
void KalmanFilter::reset() {
    // 重置状态向量为原点(0,0)
    m_x = cv::Matx21f(0.f, 0.f);
    // 重置误差协方差为单位矩阵，恢复初始不确定度
    m_p = cv::Matx22f(1.f, 0.f, 0.f, 1.f);
    // 清除初始化标记
    m_initialized = false;
}