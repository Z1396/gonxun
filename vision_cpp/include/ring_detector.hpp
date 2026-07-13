#pragma once

#include <opencv2/opencv.hpp>
#include <optional>
#include <array>
#include <map>
#include <utility>

// 圆环检测模块 - 3色环定位 + 6环识别

// 计算投放分数
// ring_id: 环号(1-6), material_fallen: 物料是否已掉落
// 返回对应分数, 无效情况返回0
int calcPlacementScore(int ring_id, bool material_fallen = false);

// 三色环检测器 - 检测3个定位圆环
class ThreeRingDetector {
public:
    ThreeRingDetector();

    // 检测3个圆环并返回6个坐标值(x1,y1,x2,y2,x3,y3), 按x排序
    // img: 输入图像(同时在原图上绘制检测结果)
    std::optional<std::array<int, 6>> detect(cv::Mat& img);

private:
    int _erodeIter;            // 腐蚀迭代次数
    cv::Mat _dilateKernel;     // 膨胀核
    int _dilateIter;           // 膨胀迭代次数
    double _claheClip;         // CLAHE对比度限制
    cv::Size _claheGrid;       // CLAHE分块大小
    cv::Mat _gradKernel;       // 形态学梯度核
    cv::Size _gaussKsize;      // 高斯模糊核大小
    double _gaussSigma;        // 高斯模糊sigma
    int _threshVal;            // 二值化阈值
    cv::Size _threshGaussKsize;// 二值化后高斯模糊核大小
    double _houghDp;           // 霍夫圆dp
    double _houghMinDist;      // 霍夫圆最小间距
    double _houghParam1;       // 霍夫圆Canny高阈值
    double _houghParam2;       // 霍夫圆累加器阈值(ALT模式为0-1比值)
    int _houghMinR;            // 霍夫圆最小半径
    int _houghMaxR;            // 霍夫圆最大半径
};

// 六环检测器 - 检测6个编号圆环
class SixRingDetector {
public:
    SixRingDetector();

    // 检测6个圆环, 返回 {环号: (x,y)} 映射, 按x排序取前6个
    // img: 输入图像(同时在原图上绘制检测结果)
    std::map<int, std::pair<int, int>> detect(cv::Mat& img);

private:
    cv::Size _blurKsize;       // 高斯模糊核大小
    double _cannyLow;          // Canny低阈值
    double _cannyHigh;         // Canny高阈值
    double _houghDp;           // 霍夫圆dp
    double _houghMinDist;      // 霍夫圆最小间距
    double _houghParam1;       // 霍夫圆Canny高阈值
    double _houghParam2;       // 霍夫圆累加器阈值
    int _houghMinR;            // 霍夫圆最小半径
    int _houghMaxR;            // 霍夫圆最大半径
};
