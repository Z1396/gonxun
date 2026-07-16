/**
 * @file color_detector.cpp
 * @brief 颜色检测模块实现文件
 * 
 * @details 本文件实现了基于HSV颜色空间的目标检测功能。
 *          核心功能：
 *          - 颜色分割：基于HSV颜色空间提取特定颜色区域
 *          - 轮廓检测：识别颜色区域的轮廓并计算中心点
 *          - 多颜色支持：支持红色、黄色、蓝色、绿色、黑色、浅蓝等
 *          - 参数配置：可调节高斯模糊、腐蚀操作、面积阈值等
 * 
 * @author 智能物流搬运系统开发团队
 * @version 1.0
 * @date 2025-01-01
 * 
 * @note 修改历史：
 *       - 2025-01-01: 初始版本，实现基本颜色检测功能
 *       - 2025-02-15: 增加红色双段HSV范围支持
 *       - 2025-03-20: 优化参数配置，提高检测稳定性
 * 
 * @see color_detector.hpp
 */
#include "color_detector.hpp"

#include <algorithm>

/**
 * @brief 构造函数，初始化颜色检测器参数
 * 
 * @details 配置图像预处理参数和各颜色的HSV范围。
 *          红色采用双段范围（色相环绕0度），其他颜色采用单段范围。
 * 
 * @note HSV 参数说明：
 *       - H (色相): 0-180 (OpenCV 将 0-360 映射到 0-180)
 *       - S (饱和度): 0-255
 *       - V (明度): 0-255
 *       
 * @note 预处理参数：
 *       - 高斯模糊核: 5x5，用于去除噪点
 *       - 腐蚀核: 3x3 全1矩阵，用于去除小噪点
 *       - 腐蚀迭代次数: 2次
 *       
 * @note 颜色范围配置：
 *       - 红色: 156-180 和 0-6（双段，因为色相环绕0度）
 *       - 黄色: 20-34
 *       - 蓝色: 100-124
 *       - 绿色: 38-90
 *       - 黑色: V 值 0-45（低明度）
 *       - 浅蓝: 85-100
 */
ColorDetector::ColorDetector() {
    // 图像预处理参数
    _blurKsize = cv::Size(5, 5);              // 高斯模糊核大小：5x5
    _erodeKernel = cv::Mat::ones(3, 3, CV_8UC1);  // 腐蚀核：3x3 全1矩阵
    _erodeIter = 2;                           // 腐蚀迭代次数：2次

    // 红色 - 两段HSV范围（色相环绕0度）
    // 红色在色相环中位于0度附近，需要两段范围：156-180 和 0-6
    _redRange1 = {cv::Scalar(156, 60, 60), cv::Scalar(180, 255, 255)};  // 高段
    _redRange2 = {cv::Scalar(0, 60, 60), cv::Scalar(6, 255, 255)};     // 低段

    // 各颜色的HSV范围配置
    _colorDist["yellow"] = {cv::Scalar(20, 100, 100), cv::Scalar(34, 255, 255)};     // 黄色
    _colorDist["blue"] = {cv::Scalar(100, 100, 45), cv::Scalar(124, 255, 255)};      // 蓝色
    _colorDist["green"] = {cv::Scalar(38, 80, 45), cv::Scalar(90, 255, 255)};        // 绿色
    _colorDist["black"] = {cv::Scalar(0, 0, 0), cv::Scalar(180, 255, 45)};           // 黑色（低明度）
    _colorDist["light_blue"] = {cv::Scalar(85, 80, 100), cv::Scalar(100, 255, 255)}; // 浅蓝
}

/**
 * @brief 生成颜色掩码
 * 
 * @details 根据指定的颜色名称，在HSV图像上生成二值掩码。
 *          红色需要特殊处理（合并两段范围），其他颜色直接使用单段范围。
 * 
 * @param hsv HSV 颜色空间的图像
 * @param color 颜色名称（"red", "yellow", "blue", "green", "black", "light_blue"）
 * 
 * @return cv::Mat 二值掩码图像（白色为目标颜色区域，黑色为其他区域）
 * 
 * @note 红色特殊处理：
 *       红色在色相环中位于0度附近，需要合并两段HSV范围：
 *       - 高段: H = 156-180
 *       - 低段: H = 0-6
 *       使用位或操作 (mask1 | mask2) 合并两段掩码
 *       
 * @see cv::inRange
 */
cv::Mat ColorDetector::_makeMask(const cv::Mat& hsv, const std::string& color) {
    // 红色特殊处理：需要合并两段范围
    if (color == "red") {
        cv::Mat mask1, mask2;
        
        // 生成高段范围掩码（156-180）
        cv::inRange(hsv, _redRange1.lower, _redRange1.upper, mask1);
        
        // 生成低段范围掩码（0-6）
        cv::inRange(hsv, _redRange2.lower, _redRange2.upper, mask2);
        
        // 合并两段掩码（位或操作）
        return mask1 | mask2;
    }
    
    // 其他颜色：查找颜色范围并生成掩码
    const auto& it = _colorDist.find(color);
    cv::Mat mask;
    cv::inRange(hsv, it->second.lower, it->second.upper, mask);
    
    return mask;
}

/**
 * @brief 图像预处理
 * 
 * @details 对输入图像进行高斯模糊、颜色空间转换、腐蚀操作。
 *          目的：去除噪点、平滑图像、增强颜色分割效果。
 * 
 * @param img 输入的 BGR 图像
 * 
 * @return cv::Mat 预处理后的 HSV 图像
 * 
 * @note 处理流程：
 *       1. 高斯模糊：去除噪点，平滑图像（核大小 5x5）
 *       2. 颜色空间转换：BGR → HSV
 *       3. 腐蚀操作：去除小噪点（核大小 3x3，迭代 2 次）
 *       
 * @see cv::GaussianBlur, cv::cvtColor, cv::erode
 */
cv::Mat ColorDetector::_preprocess(const cv::Mat& img) {
    cv::Mat blurImg, hsvImg, erodeImg;
    
    // 步骤 1: 高斯模糊（去除噪点）
    cv::GaussianBlur(img, blurImg, _blurKsize, 0);
    
    // 步骤 2: 颜色空间转换（BGR → HSV）
    cv::cvtColor(blurImg, hsvImg, cv::COLOR_BGR2HSV);
    
    // 步骤 3: 腐蚀操作（去除小噪点）
    cv::erode(hsvImg, erodeImg, _erodeKernel, cv::Point(-1, -1), _erodeIter);
    
    return erodeImg;
}

/**
 * @brief 检测指定颜色的目标中心点
 * 
 * @details 完整的颜色目标检测流程：
 *          1. 图像预处理（模糊、HSV转换、腐蚀）
 *          2. 颜色掩码生成
 *          3. 轮廓检测
 *          4. 面积过滤
 *          5. 计算最大轮廓的中心点
 * 
 * @param img 输入图像（BGR 格式）
 * @param color 颜色名称（"red", "yellow", "blue", "green", "black", "light_blue"）
 * @param min_area 最小面积阈值（像素），小于此值的轮廓将被过滤
 * @param max_area 最大面积阈值（像素），大于此值的轮廓将被过滤
 * 
 * @return std::optional<cv::Point> 检测到的目标中心点坐标
 *         - 成功: 返回中心点坐标
 *         - 失败: 返回 std::nullopt
 * 
 * @note 检测逻辑：
 *       1. 空图像检查：如果输入为空，直接返回失败
 *       2. 颜色名称校验：红色单独处理，其他颜色需要存在于配置中
 *       3. 面积过滤：超过上限或低于下限的轮廓均被丢弃
 *       4. 最大轮廓选择：返回面积最大的轮廓的中心点
 *       
 * @note 使用建议：
 *       - min_area 应根据目标大小和摄像头距离设置
 *       - max_area 用于过滤误检（如整个图像被该颜色填充）
 *       
 * @see _preprocess(), _makeMask(), cv::findContours, cv::minAreaRect
 */
std::optional<cv::Point> ColorDetector::detect(cv::Mat& img, const std::string& color,
                                               int min_area, int max_area) {
    // 空图像检查
    if (img.empty()) return std::nullopt;
    
    // 颜色名称校验（红色单独处理）
    if (color != "red" && _colorDist.find(color) == _colorDist.end()) {
        return std::nullopt;
    }

    // 步骤 1: 图像预处理（模糊、HSV转换、腐蚀）
    cv::Mat hsv = _preprocess(img);
    
    // 步骤 2: 生成颜色掩码
    cv::Mat mask = _makeMask(hsv, color);

    // 步骤 3: 轮廓检测
    std::vector<std::vector<cv::Point>> contours;
    cv::findContours(mask.clone(), contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);

    // 没有检测到轮廓
    if (contours.empty()) return std::nullopt;

    // 步骤 4: 找面积最大的轮廓
    auto maxIt = std::max_element(contours.begin(), contours.end(),
        [](const std::vector<cv::Point>& a, const std::vector<cv::Point>& b) {
            return cv::contourArea(a) < cv::contourArea(b);
        });

    // 步骤 5: 面积过滤
    double area = cv::contourArea(*maxIt);
    
    // 面积超过上限或低于下限均丢弃
    if (area > max_area) return std::nullopt;
    if (area < min_area) return std::nullopt;

    // 步骤 6: 计算最小外接矩形并返回中心点
    cv::RotatedRect rect = cv::minAreaRect(*maxIt);
    return cv::Point(static_cast<int>(rect.center.x), static_cast<int>(rect.center.y));
}
