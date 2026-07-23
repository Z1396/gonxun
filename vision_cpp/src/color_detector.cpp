/**
 * @file color_detector.cpp
 * @brief 颜色检测模块实现
 *
 * 基于 HSV 阈值分割实现6种物料颜色的定位检测。
 * 预处理: 高斯模糊 → BGR2HSV → 腐蚀去噪
 * 检测: HSV 阈值掩码 → 最大轮廓 → 最小外接矩形中心
 */
#include "color_detector.hpp"

#include <algorithm>

/**
 * @brief 构造函数，初始化 HSV 阈值和预处理参数
 * @note 红色因色相环绕(H≈0/180)分两段: [156,180] 和 [0,6]
 */
ColorDetector::ColorDetector() {
    blur_ksize_ = cv::Size(5, 5);
    erode_kernel_ = cv::Mat::ones(3, 3, CV_8UC1);
    erode_iter_ = 2;

    // 红色: 色相环绕分两段
    red_range1_ = {cv::Scalar(156, 60, 60), cv::Scalar(180, 255, 255)};
    red_range2_ = {cv::Scalar(0, 60, 60), cv::Scalar(6, 255, 255)};

    color_dist_["yellow"] = {cv::Scalar(20, 100, 100), cv::Scalar(34, 255, 255)};
    color_dist_["blue"] = {cv::Scalar(100, 100, 45), cv::Scalar(124, 255, 255)};
    color_dist_["green"] = {cv::Scalar(38, 80, 45), cv::Scalar(90, 255, 255)};
    color_dist_["black"] = {cv::Scalar(0, 0, 0), cv::Scalar(180, 255, 45)};
    color_dist_["light_blue"] = {cv::Scalar(85, 80, 100), cv::Scalar(100, 255, 255)};
}

/**
 * @brief 根据颜色名生成 HSV 掩码
 * @param hsv HSV 图像
 * @param color 颜色名
 * @return 二值掩码
 * @note 红色需合并 H[156,180] 和 H[0,6] 两段掩码
 */
cv::Mat ColorDetector::make_mask(const cv::Mat& hsv, const std::string& color) const {
    if (color == "red") {
        cv::Mat mask1, mask2;
        cv::inRange(hsv, red_range1_.lower, red_range1_.upper, mask1);
        cv::inRange(hsv, red_range2_.lower, red_range2_.upper, mask2);
        return mask1 | mask2;
    }

    const auto& it = color_dist_.find(color);
    cv::Mat mask;
    cv::inRange(hsv, it->second.lower, it->second.upper, mask);
    return mask;
}

/**
 * @brief 图像预处理: 高斯模糊 → BGR2HSV → 腐蚀
 * @param img 原始 BGR 图像
 * @return 处理后的 HSV 图像
 */
cv::Mat ColorDetector::preprocess(const cv::Mat& img) const {
    cv::Mat blur_img, hsv_img, erode_img;

    cv::GaussianBlur(img, blur_img, blur_ksize_, 0);
    cv::cvtColor(blur_img, hsv_img, cv::COLOR_BGR2HSV);
    cv::erode(hsv_img, erode_img, erode_kernel_, cv::Point(-1, -1), erode_iter_);

    return erode_img;
}

/**
 * @brief 检测指定颜色的最大轮廓中心点
 * @param img 输入图像（BGR）
 * @param color 颜色名
 * @param min_area 最小面积阈值 (px²)
 * @param max_area 最大面积阈值 (px²)
 * @return 最大轮廓的最小外接矩形中心；无符合条件轮廓返回 std::nullopt
 */
std::optional<cv::Point> ColorDetector::detect(cv::Mat& img, const std::string& color,
                                               int min_area, int max_area) {
    if (img.empty()) return std::nullopt;

    if (color != "red" && color_dist_.find(color) == color_dist_.end()) {
        return std::nullopt;
    }

    cv::Mat hsv = preprocess(img);
    cv::Mat mask = make_mask(hsv, color);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return std::nullopt;

    // 取面积最大的轮廓
    auto max_it = std::max_element(contours.begin(), contours.end(),
        [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
            return cv::contourArea(a) < cv::contourArea(b);
        });

    double area = cv::contourArea(*max_it);

    if (area > max_area) return std::nullopt;
    if (area < min_area) return std::nullopt;

    // 返回最小外接矩形的中心点
    cv::RotatedRect rect = cv::minAreaRect(*max_it);
    return cv::Point(static_cast<int>(rect.center.x), static_cast<int>(rect.center.y));
}
