/**
 * @file ring_detector.cpp
 * @brief 圆环检测模块实现
 *
 * ThreeRingDetector 使用 HOUGH_GRADIENT_ALT 高精度模式检测3个定位圆环，
 * 预处理流水线: HSV腐蚀→膨胀→灰度→CLAHE→形态学梯度→高斯模糊→
 * 对比度增强→阈值→高斯模糊→霍夫圆ALT。
 *
 * SixRingDetector 使用 HOUGH_GRADIENT 标准模式检测6个编号圆环，
 * 预处理流水线: 灰度→高斯模糊→Canny→霍夫圆标准。
 */
#include "ring_detector.hpp"

#include <algorithm>
#include <string>
#include <tuple>
#include <vector>

/**
 * @brief 计算投放分数
 * @param ring_id 环号 (1-6)
 * @param material_fallen 物料是否已掉落
 * @return 分数: R1=15, R2=10, R3=7, R4=5, R5=3, R6=1；掉落或无效返回0
 */
int calc_placement_score(int ring_id, bool material_fallen) {
    if (material_fallen || ring_id < 1 || ring_id > 6) return 0;
    static const int scores[] = {0, 15, 10, 7, 5, 3, 1};
    return scores[ring_id];
}

/**
 * @brief 构造函数，初始化3环检测参数
 * @note 使用 HOUGH_GRADIENT_ALT 模式，param2=0.95（越接近1越严格）
 */
ThreeRingDetector::ThreeRingDetector() {
    erode_iter_ = 2;
    dilate_kernel_ = cv::Mat::ones(7, 7, CV_8UC1);
    dilate_iter_ = 1;

    clahe_clip_ = 5.0;
    clahe_grid_ = cv::Size(8, 8);

    grad_kernel_ = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));

    gauss_ksize_ = cv::Size(7, 7);
    gauss_sigma_ = 3.0;

    thresh_val_ = 70;
    thresh_gauss_ksize_ = cv::Size(9, 9);

    // ==== 高精度霍夫圆 ALT 模式参数 ====
    hough_dp_ = 1.5;
    hough_min_dist_ = 50.0;
    hough_param1_ = 100.0;
    hough_param2_ = 0.95;
    hough_min_r_ = 15;
    hough_max_r_ = 50;
}

/**
 * @brief 检测3个圆环
 * @param img 输入图像（BGR），绘制红色圆环和蓝色中心点
 * @return 6元素数组 (x1,y1,x2,y2,x3,y3)，按x排序；不足3个返回 std::nullopt
 */
std::optional<std::array<int, 6>> ThreeRingDetector::detect(cv::Mat& img) {
    if (img.empty()) return std::nullopt;

    // HSV 色彩空间预处理
    cv::Mat hsv, erode_hsv, dilated, gray;
    cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);

    cv::Mat erode_kernel = cv::Mat::ones(3, 3, CV_8UC1);
    cv::erode(hsv, erode_hsv, erode_kernel, cv::Point(-1, -1), erode_iter_);
    cv::dilate(erode_hsv, dilated, dilate_kernel_, cv::Point(-1, -1), dilate_iter_);

    cv::cvtColor(dilated, gray, cv::COLOR_BGR2GRAY);

    // CLAHE 自适应直方图均衡
    cv::Mat equalized;
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(clahe_clip_, clahe_grid_);
    clahe->apply(gray, equalized);

    // 形态学梯度 → 高斯模糊 → 对比度增强 → 二值化 → 再模糊
    cv::Mat gradient, blurred, scaled, thresholded;
    cv::morphologyEx(equalized, gradient, cv::MORPH_GRADIENT, grad_kernel_);
    cv::GaussianBlur(gradient, blurred, gauss_ksize_, gauss_sigma_, gauss_sigma_);
    cv::convertScaleAbs(blurred, scaled, 4.0, 0.0);
    cv::GaussianBlur(scaled, scaled, gauss_ksize_, gauss_sigma_, gauss_sigma_);
    cv::threshold(scaled, thresholded, thresh_val_, 255, cv::THRESH_BINARY);
    cv::GaussianBlur(thresholded, thresholded, thresh_gauss_ksize_, gauss_sigma_, gauss_sigma_);

    // 霍夫圆检测 (ALT 高精度模式)
    cv::Mat circles;
    cv::HoughCircles(thresholded, circles, cv::HOUGH_GRADIENT_ALT,
                     hough_dp_, hough_min_dist_, hough_param1_, hough_param2_,
                     hough_min_r_, hough_max_r_);

    if (circles.empty() || circles.cols != 3) return std::nullopt;

    // 提取圆心坐标并绘制
    std::vector<std::pair<int, int>> pts;
    for (int i = 0; i < circles.cols; i++) {
        cv::Vec3f c = circles.at<cv::Vec3f>(0, i);
        int cx = cvRound(c[0]);
        int cy = cvRound(c[1]);
        int r = cvRound(c[2]);

        cv::circle(img, cv::Point(cx, cy), r, cv::Scalar(0, 0, 255), 2);
        cv::circle(img, cv::Point(cx, cy), 2, cv::Scalar(255, 0, 0), 2);

        pts.emplace_back(cx, cy);
    }

    // 按 x 坐标排序，取前3个
    std::sort(pts.begin(), pts.end());

    std::array<int, 6> result;
    for (int i = 0; i < 3; i++) {
        result[i * 2] = pts[i].first;
        result[i * 2 + 1] = pts[i].second;
    }
    return result;
}

/**
 * @brief 构造函数，初始化6环检测参数
 * @note 使用 HOUGH_GRADIENT 标准模式，param2=30 为圆检测累加器阈值
 */
SixRingDetector::SixRingDetector() {
    blur_ksize_ = cv::Size(5, 5);
    canny_low_ = 50.0;
    canny_high_ = 150.0;
    hough_dp_ = 1.0;
    hough_min_dist_ = 40.0;
    hough_param1_ = 100.0;
    hough_param2_ = 30.0;
    hough_min_r_ = 20;
    hough_max_r_ = 120;
}

/**
 * @brief 检测6个圆环
 * @param img 输入图像（BGR），绘制紫色圆环和编号
 * @return 环号(1-6)→中心坐标映射；不足6个返回空 map
 */
std::map<int, std::pair<int, int>> SixRingDetector::detect(cv::Mat& img) {
    std::map<int, std::pair<int, int>> result;
    if (img.empty()) return result;

    // 灰度 → 高斯模糊 → Canny 边缘检测
    cv::Mat gray, blurred, edges;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, blurred, blur_ksize_, 0);
    cv::Canny(blurred, edges, canny_low_, canny_high_);

    // 霍夫圆检测 (标准模式)
    cv::Mat circles;
    cv::HoughCircles(edges, circles, cv::HOUGH_GRADIENT,
                     hough_dp_, hough_min_dist_, hough_param1_, hough_param2_,
                     hough_min_r_, hough_max_r_);

    if (circles.empty()) return result;

    // 提取圆心坐标
    std::vector<std::tuple<int, int, int>> detected;
    for (int i = 0; i < circles.cols; i++) {
        cv::Vec3f c = circles.at<cv::Vec3f>(0, i);
        detected.emplace_back(static_cast<int>(c[0]), static_cast<int>(c[1]),
                              static_cast<int>(c[2]));
    }

    // 按 x 坐标排序
    std::sort(detected.begin(), detected.end(),
        [](const std::tuple<int, int, int>& a, const std::tuple<int, int, int>& b) {
            return std::get<0>(a) < std::get<0>(b);
        });

    // 取前6个，编号1-6
    if (detected.size() < 6) return result;

    for (int i = 0; i < 6; i++) {
        int x = std::get<0>(detected[i]);
        int y = std::get<1>(detected[i]);
        int r = std::get<2>(detected[i]);

        result[i + 1] = std::make_pair(x, y);

        cv::circle(img, cv::Point(x, y), r, cv::Scalar(255, 0, 255), 2);
        cv::putText(img, std::to_string(i + 1), cv::Point(x - 5, y + 5),
                    cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(255, 0, 255), 2);
    }

    return result;
}
