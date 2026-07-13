#include "obstacle_detector.hpp"

ObstacleDetector::ObstacleDetector(int min_radius, int max_radius, int min_area)
    : _minRadius(min_radius), _maxRadius(max_radius), _minArea(min_area) {
    // 黑色HSV阈值
    _lowerBlack = cv::Scalar(0, 0, 0);
    _upperBlack = cv::Scalar(180, 80, 60);
    // 形态学操作核 (5x5 全1)
    _morphKernel = cv::Mat::ones(5, 5, CV_8UC1);
    // 霍夫圆参数
    _houghDp = 1.0;
    _houghMinDist = 50.0;
    _houghParam1 = 50.0;
    _houghParam2 = 15.0;
}

std::vector<std::tuple<int, int, int>> ObstacleDetector::detect(cv::Mat& img) {
    std::vector<std::tuple<int, int, int>> obstacles;
    if (img.empty()) return obstacles;

    // BGR -> HSV, 提取黑色区域掩膜
    cv::Mat hsv, mask;
    cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
    cv::inRange(hsv, _lowerBlack, _upperBlack, mask);

    // 形态学开运算(去噪) + 闭运算(填洞)
    cv::morphologyEx(mask, mask, cv::MORPH_OPEN, _morphKernel);
    cv::morphologyEx(mask, mask, cv::MORPH_CLOSE, _morphKernel);

    // 霍夫圆检测
    cv::Mat circles;
    cv::HoughCircles(mask, circles, cv::HOUGH_GRADIENT,
                     _houghDp, _houghMinDist, _houghParam1, _houghParam2,
                     _minRadius, _maxRadius);

    if (circles.empty()) return obstacles;

    // 按面积过滤
    for (int i = 0; i < circles.cols; i++) {
        cv::Vec3f c = circles.at<cv::Vec3f>(0, i);
        int cx = static_cast<int>(c[0]);
        int cy = static_cast<int>(c[1]);
        int r = static_cast<int>(c[2]);
        double area = CV_PI * r * r;
        if (area >= _minArea) {
            obstacles.emplace_back(cx, cy, r);
        }
    }
    return obstacles;
}

std::vector<std::tuple<int, int, int>> ObstacleDetector::detectAndDraw(cv::Mat& img) {
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
