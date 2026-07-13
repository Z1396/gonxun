#include "ring_detector.hpp"

#include <algorithm>
#include <vector>
#include <tuple>
#include <string>

// 环号到分数的映射 (索引0未用, 1-6对应分数)
int calcPlacementScore(int ring_id, bool material_fallen) {
    if (material_fallen || ring_id < 1 || ring_id > 6) return 0;
    static const int scores[] = {0, 15, 10, 7, 5, 3, 1};
    return scores[ring_id];
}

ThreeRingDetector::ThreeRingDetector() {
    _erodeIter = 2;
    _dilateKernel = cv::Mat::ones(7, 7, CV_8UC1);
    _dilateIter = 1;
    _claheClip = 5.0;
    _claheGrid = cv::Size(8, 8);
    _gradKernel = cv::getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    _gaussKsize = cv::Size(7, 7);
    _gaussSigma = 3.0;
    _threshVal = 70;
    _threshGaussKsize = cv::Size(9, 9);
    _houghDp = 1.5;
    _houghMinDist = 50.0;
    _houghParam1 = 100.0;
    _houghParam2 = 0.95;
    _houghMinR = 15;
    _houghMaxR = 50;
}

std::optional<std::array<int, 6>> ThreeRingDetector::detect(cv::Mat& img) {
    if (img.empty()) return std::nullopt;

    // BGR -> HSV -> 腐蚀 -> 膨胀 -> 灰度
    cv::Mat hsv, erodeHsv, dilated, gray;
    cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);
    cv::Mat erodeKernel = cv::Mat::ones(3, 3, CV_8UC1);
    cv::erode(hsv, erodeHsv, erodeKernel, cv::Point(-1, -1), _erodeIter);
    cv::dilate(erodeHsv, dilated, _dilateKernel, cv::Point(-1, -1), _dilateIter);
    cv::cvtColor(dilated, gray, cv::COLOR_BGR2GRAY);

    // CLAHE 直方图均衡
    cv::Mat equalized;
    cv::Ptr<cv::CLAHE> clahe = cv::createCLAHE(_claheClip, _claheGrid);
    clahe->apply(gray, equalized);

    // 形态学梯度 -> 高斯模糊 -> 增益 -> 高斯模糊 -> 二值化 -> 高斯模糊
    cv::Mat gradient, blurred, scaled, thresholded;
    cv::morphologyEx(equalized, gradient, cv::MORPH_GRADIENT, _gradKernel);
    cv::GaussianBlur(gradient, blurred, _gaussKsize, _gaussSigma, _gaussSigma);
    cv::convertScaleAbs(blurred, scaled, 4.0, 0.0);
    cv::GaussianBlur(scaled, scaled, _gaussKsize, _gaussSigma, _gaussSigma);
    cv::threshold(scaled, thresholded, _threshVal, 255, cv::THRESH_BINARY);
    cv::GaussianBlur(thresholded, thresholded, _threshGaussKsize, _gaussSigma, _gaussSigma);

    // 霍夫圆检测 (ALT模式)
    cv::Mat circles;
    cv::HoughCircles(thresholded, circles, cv::HOUGH_GRADIENT_ALT,
                     _houghDp, _houghMinDist, _houghParam1, _houghParam2,
                     _houghMinR, _houghMaxR);

    // 仅当检测到恰好3个圆时有效
    if (circles.empty() || circles.cols != 3) return std::nullopt;

    // 收集圆心坐标并绘制
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

    // 按坐标排序(先x后y, 与Python元组排序一致)
    std::sort(pts.begin(), pts.end());

    // 展平为 (x1,y1,x2,y2,x3,y3)
    std::array<int, 6> result;
    for (int i = 0; i < 3; i++) {
        result[i * 2] = pts[i].first;
        result[i * 2 + 1] = pts[i].second;
    }
    return result;
}

SixRingDetector::SixRingDetector() {
    _blurKsize = cv::Size(5, 5);
    _cannyLow = 50.0;
    _cannyHigh = 150.0;
    _houghDp = 1.0;
    _houghMinDist = 40.0;
    _houghParam1 = 100.0;
    _houghParam2 = 30.0;
    _houghMinR = 20;
    _houghMaxR = 120;
}

std::map<int, std::pair<int, int>> SixRingDetector::detect(cv::Mat& img) {
    std::map<int, std::pair<int, int>> result;
    if (img.empty()) return result;

    // 灰度 -> 高斯模糊 -> Canny边缘
    cv::Mat gray, blurred, edges;
    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    cv::GaussianBlur(gray, blurred, _blurKsize, 0);
    cv::Canny(blurred, edges, _cannyLow, _cannyHigh);

    // 霍夫圆检测
    cv::Mat circles;
    cv::HoughCircles(edges, circles, cv::HOUGH_GRADIENT,
                     _houghDp, _houghMinDist, _houghParam1, _houghParam2,
                     _houghMinR, _houghMaxR);

    if (circles.empty()) return result;

    // 收集检测结果
    std::vector<std::tuple<int, int, int>> detected;
    for (int i = 0; i < circles.cols; i++) {
        cv::Vec3f c = circles.at<cv::Vec3f>(0, i);
        detected.emplace_back(static_cast<int>(c[0]), static_cast<int>(c[1]),
                              static_cast<int>(c[2]));
    }

    // 按x坐标排序
    std::sort(detected.begin(), detected.end(),
        [](const std::tuple<int, int, int>& a, const std::tuple<int, int, int>& b) {
            return std::get<0>(a) < std::get<0>(b);
        });

    // 不足6个返回空
    if (detected.size() < 6) return result;

    // 取前6个, 编号1-6
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
