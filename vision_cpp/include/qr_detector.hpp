/**
 * @file qr_detector.hpp
 * @brief 二维码识别与任务码解析
 *
 * QRDetector 封装 OpenCV QRCodeDetector 实现二维码检测与解码，
 * 支持在图像上绘制边框和中心点。TaskCodeParser 将 QR 码字符串
 * 解析为两批次物料颜色和投放位置的 TaskCode 结构。
 *
 * QR 码格式: "XXX+YYY+ZZZ+WWW"（4段3位数字，'+'分隔）
 *   XXX: 第1批次颜色编号 (1-6: 红/黄/蓝/绿/黑/浅蓝)
 *   YYY: 第1批次投放位置编号 (1-6)
 *   ZZZ: 第2批次颜色编号
 *   WWW: 第2批次投放位置编号
 */
#pragma once

#include <optional>
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

/**
 * @brief 任务码解析结果
 *
 * 每个向量的3个元素分别对应3个物料槽位的颜色/位置编号。
 * 编号范围: 1~6（1=红, 2=黄, 3=蓝, 4=绿, 5=黑, 6=浅蓝）
 */
struct TaskCode {
    std::vector<int> batch1_colors;    ///< 第1批次颜色编号 [3]
    std::vector<int> batch1_positions; ///< 第1批次投放位置 [3]
    std::vector<int> batch2_colors;    ///< 第2批次颜色编号 [3]
    std::vector<int> batch2_positions; ///< 第2批次投放位置 [3]
};

/**
 * @brief 二维码检测器，封装 OpenCV QRCodeDetector
 */
class QRDetector {
public:
    QRDetector();

    /**
     * @brief 检测并解码二维码
     * @param img 输入图像（BGR），不可为空
     * @return 解码字符串；未检测到返回 std::nullopt
     */
    [[nodiscard]] std::optional<std::string> detect(cv::Mat& img);

    /**
     * @brief 检测并解码二维码，同时在图像上绘制边框和中心点
     * @param img 输入图像（BGR），将被修改
     * @return 解码字符串；未检测到返回 std::nullopt
     */
    [[nodiscard]] std::optional<std::string> detect_and_draw(cv::Mat& img);

private:
    cv::QRCodeDetector decoder_; ///< OpenCV QR 码检测器
};

/**
 * @brief 任务码解析器，将 QR 码字符串转为 TaskCode
 */
class TaskCodeParser {
public:
    /**
     * @brief 解析二维码字符串
     * @param qr_data 格式: "XXX+YYY+ZZZ+WWW"，每段3位数字
     * @return 解析结果；格式非法或数值越界返回 std::nullopt
     * @note 每位数字须在 1~6 范围内
     */
    [[nodiscard]] static std::optional<TaskCode> parse(const std::string& qr_data);
};
