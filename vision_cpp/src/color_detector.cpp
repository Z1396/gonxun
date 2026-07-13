#include "color_detector.hpp"

#include <algorithm>

ColorDetector::ColorDetector() {
    // 高斯模糊核大小
    _blurKsize = cv::Size(5, 5);
    // 腐蚀核 (3x3 全1)
    _erodeKernel = cv::Mat::ones(3, 3, CV_8UC1);
    // 腐蚀迭代次数
    _erodeIter = 2;

    // 红色 - 两段HSV范围(色相环绕0度)
    _redRange1 = {cv::Scalar(156, 60, 60), cv::Scalar(180, 255, 255)};
    _redRange2 = {cv::Scalar(0, 60, 60), cv::Scalar(6, 255, 255)};

    // 黄色
    _colorDist["yellow"] = {cv::Scalar(20, 100, 100), cv::Scalar(34, 255, 255)};
    // 蓝色
    _colorDist["blue"] = {cv::Scalar(100, 100, 45), cv::Scalar(124, 255, 255)};
    // 绿色
    _colorDist["green"] = {cv::Scalar(38, 80, 45), cv::Scalar(90, 255, 255)};
    // 黑色
    _colorDist["black"] = {cv::Scalar(0, 0, 0), cv::Scalar(180, 255, 45)};
    // 浅蓝
    _colorDist["light_blue"] = {cv::Scalar(85, 80, 100), cv::Scalar(100, 255, 255)};
}

cv::Mat ColorDetector::_makeMask(const cv::Mat& hsv, const std::string& color) {
    if (color == "red") {
        // 红色需要合并两段范围(色相环绕0度)
        cv::Mat mask1, mask2;
        cv::inRange(hsv, _redRange1.lower, _redRange1.upper, mask1);
        cv::inRange(hsv, _redRange2.lower, _redRange2.upper, mask2);
        return mask1 | mask2;
    }
    const auto& it = _colorDist.find(color);
    cv::Mat mask;
    cv::inRange(hsv, it->second.lower, it->second.upper, mask);
    return mask;
}

cv::Mat ColorDetector::_preprocess(const cv::Mat& img) {
    cv::Mat blurImg, hsvImg, erodeImg;
    cv::GaussianBlur(img, blurImg, _blurKsize, 0);
    cv::cvtColor(blurImg, hsvImg, cv::COLOR_BGR2HSV);
    cv::erode(hsvImg, erodeImg, _erodeKernel, cv::Point(-1, -1), _erodeIter);
    return erodeImg;
}

std::optional<cv::Point> ColorDetector::detect(cv::Mat& img, const std::string& color,
                                               int min_area, int max_area) {
    if (img.empty()) return std::nullopt;
    // 校验颜色名(红色单独处理)
    if (color != "red" && _colorDist.find(color) == _colorDist.end()) {
        return std::nullopt;
    }

    cv::Mat hsv = _preprocess(img);
    cv::Mat mask = _makeMask(hsv, color);

    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    if (contours.empty()) return std::nullopt;

    // 找面积最大的轮廓
    auto maxIt = std::max_element(contours.begin(), contours.end(),
        [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
            return cv::contourArea(a) < cv::contourArea(b);
        });

    double area = cv::contourArea(*maxIt);
    // 面积过滤: 超过上限或低于下限均丢弃
    if (area > max_area) return std::nullopt;
    if (area < min_area) return std::nullopt;

    cv::RotatedRect rect = cv::minAreaRect(*maxIt);
    return cv::Point(static_cast<int>(rect.center.x), static_cast<int>(rect.center.y));
}
