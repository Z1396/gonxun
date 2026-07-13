#pragma once

#include <opencv2/opencv.hpp>
#include <optional>
#include <string>
#include <vector>

// 二维码识别与任务码解析

// 任务码解析结果结构体
struct TaskCode {
    std::vector<int> batch1_colors;     // 第一批物料颜色编号 (1-6)
    std::vector<int> batch1_positions;  // 第一批投放环号 (1-6)
    std::vector<int> batch2_colors;     // 第二批物料颜色编号 (1-6)
    std::vector<int> batch2_positions;  // 第二批投放环号 (1-6)
};

// 二维码检测器
class QRDetector {
public:
    QRDetector();

    // 检测并解码二维码, 返回解码字符串
    std::optional<std::string> detect(cv::Mat& img);

    // 检测并解码二维码, 同时在图像上绘制边框和中心点
    std::optional<std::string> detectAndDraw(cv::Mat& img);

private:
    cv::QRCodeDetector _decoder;
};

// 任务码解析器
class TaskCodeParser {
public:
    // 解析二维码字符串
    // 格式: "XXX+YYY+ZZZ+WWW", 每段3位数字(1-6)
    // 分别对应: 第一批颜色, 第一批位置, 第二批颜色, 第二批位置
    // 返回解析结果, 无效返回 std::nullopt
    static std::optional<TaskCode> parse(const std::string& qr_data);
};
