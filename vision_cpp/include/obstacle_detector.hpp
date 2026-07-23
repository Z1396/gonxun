/**
 * @file obstacle_detector.hpp
 * @brief 障碍物检测模块，基于黑色HSV阈值 + 形态学处理 + 霍夫圆检测
 *
 * 通过 HSV 黑色阈值提取障碍物区域，经形态学开闭运算去噪后，
 * 使用霍夫圆检测定位圆形障碍物的圆心和半径。
 * 黑色阈值: H[0,180], S[0,80], V[0,60]
 */
#pragma once

#include <tuple>
#include <vector>
#include <opencv2/opencv.hpp>

/**
 * @brief 障碍物检测器
 *
 * 检测流程: BGR→HSV→黑色阈值→开运算→闭运算→霍夫圆→面积过滤
 */
class ObstacleDetector {
public:
    /**
     * @brief 构造函数
     * @param min_radius 最小圆半径 (px)，默认 15
     * @param max_radius 最大圆半径 (px)，默认 35
     * @param min_area 最小圆面积 (px²)，默认 500
     */
    explicit ObstacleDetector(int min_radius = 15, int max_radius = 35, int min_area = 500);

    /**
     * @brief 检测障碍物，返回 (x, y, r) 列表
     * @param img 输入图像（BGR）
     * @return 障碍物列表，每个元素为 (圆心x, 圆心y, 半径r)
     */
    [[nodiscard]] std::vector<std::tuple<int, int, int>> detect(cv::Mat& img);

    /**
     * @brief 检测障碍物并在图像上绘制
     * @param img 输入图像（BGR），将被修改：红色圆环 + 绿色中心点
     * @return 障碍物列表，每个元素为 (圆心x, 圆心y, 半径r)
     */
    [[nodiscard]] std::vector<std::tuple<int, int, int>> detect_and_draw(cv::Mat& img);

private:
    cv::Scalar lower_black_;   ///< 黑色 HSV 下界
    cv::Scalar upper_black_;   ///< 黑色 HSV 上界
    cv::Mat morph_kernel_;     ///< 形态学运算核 (5×5 全1)
    double hough_dp_;          ///< 霍夫圆累加器分辨率比
    double hough_min_dist_;    ///< 霍夫圆最小圆心间距
    double hough_param1_;      ///< Canny 高阈值
    double hough_param2_;      ///< 圆检测阈值
    int min_radius_;           ///< 最小半径 (px)
    int max_radius_;           ///< 最大半径 (px)
    int min_area_;             ///< 最小面积 (px²)
};
