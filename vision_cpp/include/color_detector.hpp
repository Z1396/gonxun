#pragma once

#include <opencv2/opencv.hpp>
#include <optional>
#include <string>
#include <unordered_map>

// 颜色识别模块 - 6种物料颜色HSV阈值
// 支持: red(红), yellow(黄), blue(蓝), green(绿), black(黑), light_blue(浅蓝)
class ColorDetector {
public:
    ColorDetector();

    // 检测指定颜色的最大轮廓中心点
    // img: 输入图像(BGR), color: 颜色名, min_area/max_area: 面积过滤阈值
    // 返回中心点坐标, 若无符合条件轮廓返回 std::nullopt
    std::optional<cv::Point> detect(cv::Mat& img, const std::string& color,
                                    int min_area = 2000, int max_area = 10000);

private:
    // HSV阈值范围
    struct HsvThreshold {
        cv::Scalar lower;
        cv::Scalar upper;
    };

    // 生成颜色掩膜
    cv::Mat _makeMask(const cv::Mat& hsv, const std::string& color);
    // 图像预处理: 高斯模糊 -> HSV转换 -> 腐蚀
    cv::Mat _preprocess(const cv::Mat& img);

    std::unordered_map<std::string, HsvThreshold> _colorDist;
    // 红色需要两段HSV范围(跨越0度)
    HsvThreshold _redRange1;
    HsvThreshold _redRange2;

    cv::Size _blurKsize;      // 高斯模糊核大小
    cv::Mat _erodeKernel;     // 腐蚀核
    int _erodeIter;           // 腐蚀迭代次数
};
