/**
 * @file ring_detector.hpp
 * @brief 圆环检测模块：3色环定位 + 6环识别
 *
 * ThreeRingDetector 用于检测场地上的3个定位圆环，返回排序后的
 * 6个坐标值(x1,y1,x2,y2,x3,y3)。SixRingDetector 用于检测
 * 6个编号圆环，返回 {环号: (x,y)} 映射。
 *
 * 两者均使用霍夫圆检测，但预处理流程不同:
 * - ThreeRingDetector: HSV→腐蚀→膨胀→灰度→CLAHE→形态学梯度→阈值→霍夫ALT
 * - SixRingDetector: 灰度→高斯模糊→Canny→霍夫标准
 */
#pragma once

#include <array>
#include <map>
#include <optional>
#include <utility>
#include <opencv2/opencv.hpp>

/**
 * @brief 计算投放分数
 * @param ring_id 环号 (1-6)，1号环分值最高
 * @param material_fallen 物料是否已掉落，掉落则得0分
 * @return 对应分数: R1=15, R2=10, R3=7, R4=5, R5=3, R6=1；无效返回0
 */
[[nodiscard]] int calc_placement_score(int ring_id, bool material_fallen = false);

/**
 * @brief 3色环检测器，使用 HOUGH_GRADIENT_ALT 高精度模式
 *
 * 预处理流水线: HSV腐蚀→膨胀→灰度→CLAHE→形态学梯度→
 * 高斯模糊×2→对比度增强→阈值→高斯模糊→霍夫圆ALT
 */
class ThreeRingDetector {
public:
    ThreeRingDetector();

    /**
     * @brief 检测3个圆环并返回6个坐标值(x1,y1,x2,y2,x3,y3)，按x排序
     * @param img 输入图像（BGR），同时在原图上绘制红色圆环和蓝色中心点
     * @return 6元素数组；检测不足3个圆返回 std::nullopt
     */
    [[nodiscard]] std::optional<std::array<int, 6>> detect(cv::Mat& img);

private:
    int erode_iter_;          ///< HSV 腐蚀迭代次数
    cv::Mat dilate_kernel_;   ///< 膨胀核
    int dilate_iter_;         ///< 膨胀迭代次数
    double clahe_clip_;       ///< CLAHE 对比度限制阈值
    cv::Size clahe_grid_;     ///< CLAHE 网格尺寸
    cv::Mat grad_kernel_;     ///< 形态学梯度核（椭圆5×5）
    cv::Size gauss_ksize_;    ///< 高斯模糊核尺寸
    double gauss_sigma_;      ///< 高斯模糊标准差
    int thresh_val_;          ///< 二值化阈值
    cv::Size thresh_gauss_ksize_; ///< 阈值后高斯模糊核
    double hough_dp_;         ///< 霍夫圆累加器分辨率比
    double hough_min_dist_;   ///< 霍夫圆最小圆心间距
    double hough_param1_;     ///< Canny 高阈值 (ALT 模式)
    double hough_param2_;     ///< 圆检测阈值 (ALT 模式，越小越敏感)
    int hough_min_r_;         ///< 最小半径 (px)
    int hough_max_r_;         ///< 最大半径 (px)
};

/**
 * @brief 6环检测器，使用 HOUGH_GRADIENT 标准模式
 *
 * 预处理流水线: 灰度→高斯模糊→Canny→霍夫圆标准模式
 */
class SixRingDetector {
public:
    SixRingDetector();

    /**
     * @brief 检测6个圆环，返回 {环号: (x,y)} 映射，按x排序取前6个
     * @param img 输入图像（BGR），同时在原图上绘制紫色圆环和编号
     * @return 环号(1-6)→中心坐标映射；检测不足6个返回空 map
     */
    [[nodiscard]] std::map<int, std::pair<int, int>> detect(cv::Mat& img);

private:
    cv::Size blur_ksize_;     ///< 高斯模糊核尺寸
    double canny_low_;        ///< Canny 低阈值
    double canny_high_;       ///< Canny 高阈值
    double hough_dp_;         ///< 霍夫圆累加器分辨率比
    double hough_min_dist_;   ///< 霍夫圆最小圆心间距
    double hough_param1_;     ///< Canny 高阈值
    double hough_param2_;     ///< 圆检测阈值
    int hough_min_r_;         ///< 最小半径 (px)
    int hough_max_r_;         ///< 最大半径 (px)
};
