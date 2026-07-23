/**
 * @file obstacle_detector.cpp
 * @brief 障碍物检测模块实现
 *
 * 检测流程: BGR→HSV→黑色阈值分割→形态学开闭运算去噪→
 * 霍夫圆检测→面积过滤。黑色HSV阈值: H[0,180], S[0,80], V[0,60]。
 */
#include "obstacle_detector.hpp"

/**
 * @brief 构造函数，初始化黑色阈值和检测参数
 * @param min_radius 最小圆半径 (px)
 * @param max_radius 最大圆半径 (px)
 * @param min_area 最小圆面积 (px²)
 */
ObstacleDetector::ObstacleDetector(int min_radius, int max_radius, int min_area)
    : min_radius_(min_radius),
      max_radius_(max_radius),
      min_area_(min_area) {
    // 黑色 HSV 阈值: 低饱和度+低明度
    lower_black_ = cv::Scalar(0, 0, 0);
    upper_black_ = cv::Scalar(180, 80, 60);

    morph_kernel_ = cv::Mat::ones(5, 5, CV_8UC1);

    hough_dp_ = 1.0;
    hough_min_dist_ = 50.0;
    hough_param1_ = 50.0;
    hough_param2_ = 15.0;
}

/**
 * @brief 检测障碍物
 * @param img 输入图像（BGR）
 * @return 障碍物列表，每个元素为 (圆心x, 圆心y, 半径r)
 */
std::vector<std::tuple<int, int, int>> ObstacleDetector::detect(cv::Mat& img) {
    std::vector<std::tuple<int, int, int>> obstacles;
    if (img.empty())
        return obstacles;

    // HSV 黑色阈值分割
    cv::Mat hsv, mask;
    cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
    cv::inRange(hsv, lower_black_, upper_black_, mask);

    // 形态学开运算去小噪 + 闭运算填孔
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, morph_kernel_);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, morph_kernel_);

    // 霍夫圆检测
    cv::Mat circles;
    cv::HoughCircles(
        mask,
        circles,
        cv::HOUGH_GRADIENT,
        hough_dp_,
        hough_min_dist_,
        hough_param1_,
        hough_param2_,
        min_radius_,
        max_radius_
    );

    if (circles.empty())
        return obstacles;

    // 面积过滤
    for (int i = 0; i < circles.cols; i++) {
        cv::Vec3f c = circles.at<cv::Vec3f>(0, i);
        int cx = static_cast<int>(c[0]);
        int cy = static_cast<int>(c[1]);
        int r = static_cast<int>(c[2]);
        double area = CV_PI * r * r;
        if (area >= min_area_) {
            obstacles.emplace_back(cx, cy, r);
        }
    }
    return obstacles;
}

/**
 * @brief 检测障碍物并在图像上绘制
 * @param img 输入图像（BGR），绘制红色圆环和绿色中心点
 * @return 障碍物列表
 */
std::vector<std::tuple<int, int, int>> ObstacleDetector::detect_and_draw(cv::Mat& img) {
    std::vector<std::tuple<int, int, int>> obstacles = detect(img);

    for (const auto& obs : obstacles) {
        int cx = std::get<0>(obs);
        int cy = std::get<1>(obs);
        int r = std::get<2>(obs);
        cv::circle(img, cv::Point(cx, cy), r, cv::Scalar(0, 0, 255), 2);
        cv::circle(img, cv::Point(cx, cy), 2, cv::Scalar(0, 255, 0), -1);
    }
    return obstacles;
}
