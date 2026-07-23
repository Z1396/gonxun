/**
 * @file color_detector.hpp
 * @brief 颜色识别模块，6种物料颜色HSV阈值检测
 *
 * 基于 HSV 色彩空间阈值分割 + 最大轮廓提取实现颜色定位。
 * 支持6种颜色: red, yellow, blue, green, black, light_blue。
 * 红色因色相环绕(H=0/180)需两段阈值合并。
 *
 * HSV 阈值一览:
 *   red:        H[156,180]∪[0,6],   S[60,255], V[60,255]
 *   yellow:     H[20,34],           S[100,255], V[100,255]
 *   blue:       H[100,124],         S[100,255], V[45,255]
 *   green:      H[38,90],           S[80,255],  V[45,255]
 *   black:      H[0,180],           S[0,255],   V[0,45]
 *   light_blue: H[85,100],          S[80,255],  V[100,255]
 */
#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <opencv2/opencv.hpp>

/**
 * @brief 颜色检测器，HSV 阈值分割 + 最大轮廓定位
 */
class ColorDetector {
public:
    ColorDetector();

    /**
     * @brief 检测指定颜色的最大轮廓中心点
     * @param img 输入图像（BGR），函数内部会做 HSV 预处理
     * @param color 颜色名: red/yellow/blue/green/black/light_blue
     * @param min_area 最小面积阈值 (px²)，默认 2000
     * @param max_area 最大面积阈值 (px²)，默认 10000
     * @return 最大轮廓的最小外接矩形中心；无符合条件轮廓返回 std::nullopt
     */
    [[nodiscard]] std::optional<cv::Point> detect(cv::Mat& img, const std::string& color,
                                                  int min_area = 2000, int max_area = 10000);

private:
    /** @brief HSV 阈值范围 */
    struct HsvThreshold {
        cv::Scalar lower; ///< 下界 (H, S, V)
        cv::Scalar upper; ///< 上界 (H, S, V)
    };

    /**
     * @brief 根据颜色名生成 HSV 掩码
     * @param hsv HSV 图像
     * @param color 颜色名
     * @return 二值掩码图像
     * @note 红色需合并两段 H 阈值掩码
     */
    [[nodiscard]] cv::Mat make_mask(const cv::Mat& hsv, const std::string& color) const;

    /**
     * @brief 图像预处理: 高斯模糊 → BGR2HSV → 腐蚀
     * @param img 原始 BGR 图像
     * @return 处理后的 HSV 图像
     */
    [[nodiscard]] cv::Mat preprocess(const cv::Mat& img) const;

    std::unordered_map<std::string, HsvThreshold> color_dist_; ///< 非红色的 HSV 阈值映射
    HsvThreshold red_range1_;  ///< 红色高色相段 H[156,180]
    HsvThreshold red_range2_;  ///< 红色低色相段 H[0,6]

    cv::Size blur_ksize_;      ///< 高斯模糊核尺寸
    cv::Mat erode_kernel_;     ///< 腐蚀核
    int erode_iter_;           ///< 腐蚀迭代次数
};
