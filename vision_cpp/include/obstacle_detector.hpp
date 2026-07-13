#pragma once

#include <opencv2/opencv.hpp>
#include <vector>
#include <tuple>

// 障碍物检测模块
// 基于黑色HSV阈值 + 形态学处理 + 霍夫圆检测
class ObstacleDetector {
public:
    // min_radius/max_radius: 霍夫圆半径范围, min_area: 面积过滤阈值
    ObstacleDetector(int min_radius = 15, int max_radius = 35, int min_area = 500);

    // 检测障碍物, 返回 (x, y, r) 列表
    std::vector<std::tuple<int, int, int>> detect(cv::Mat& img);

    // 检测障碍物并在图像上绘制, 返回 (x, y, r) 列表
    std::vector<std::tuple<int, int, int>> detectAndDraw(cv::Mat& img);

private:
    cv::Scalar _lowerBlack;    // 黑色HSV下界
    cv::Scalar _upperBlack;    // 黑色HSV上界
    cv::Mat _morphKernel;      // 形态学操作核
    double _houghDp;           // 霍夫圆dp
    double _houghMinDist;      // 霍夫圆最小间距
    double _houghParam1;       // 霍夫圆Canny高阈值
    double _houghParam2;       // 霍夫圆累加器阈值
    int _minRadius;            // 最小半径
    int _maxRadius;            // 最大半径
    int _minArea;              // 最小面积阈值
};
