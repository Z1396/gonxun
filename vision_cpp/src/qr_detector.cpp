/**
 * @file qr_detector.cpp
 * @brief 二维码识别与任务码解析实现
 *
 * QRDetector 使用 OpenCV QRCodeDetector 进行检测与解码，
 * detect_and_draw 在图像上绘制蓝色边框和绿色中心点。
 * TaskCodeParser 将 "XXX+YYY+ZZZ+WWW" 格式字符串解析为 TaskCode。
 */
#include "qr_detector.hpp"

#include <cctype>

/** @brief 默认构造函数 */
QRDetector::QRDetector() = default;

/**
 * @brief 检测并解码二维码
 * @param img 输入图像（BGR）
 * @return 解码字符串；未检测到或图像为空返回 std::nullopt
 */
std::optional<std::string> QRDetector::detect(cv::Mat& img) 
{
    if (img.empty())
        return std::nullopt;

    cv::Mat points;
    /*5. 和你代码链路关联
    相机读到 frame → 丢进 detectAndDecode → 判断返回字符串非空 → 调用你注册的 lambda 回调，把二维码数据抛给上层 UI / 串口。*/
    std::string data = decoder_.detectAndDecode(img, points);

    if (data.empty())
        return std::nullopt;

    return data;
}

/**
 * @brief 检测并解码二维码，绘制边框和中心点
 * @param img 输入图像（BGR），将被修改
 * @return 解码字符串；未检测到返回 std::nullopt
 * @note 蓝色线段连接角点，绿色实心圆标注中心
 */
std::optional<std::string> QRDetector::detect_and_draw(cv::Mat& img) {
    if (img.empty())
        return std::nullopt;

    cv::Mat points;
    std::string data = decoder_.detectAndDecode(img, points);
    if (data.empty() || points.empty())
        return std::nullopt;

    // 绘制蓝色边框：连接相邻角点
    int n = static_cast<int>(points.total());
    for (int j = 0; j < n; j++) {
        cv::Point2f p1 = points.at<cv::Point2f>(j);
        cv::Point2f p2 = points.at<cv::Point2f>((j + 1) % n);

        cv::line(img,
                 cv::Point(static_cast<int>(p1.x), static_cast<int>(p1.y)),
                 cv::Point(static_cast<int>(p2.x), static_cast<int>(p2.y)),
                 cv::Scalar(255, 0, 0), 3);
    }

    // 计算并绘制绿色中心点
    float sum_x = 0.0f, sum_y = 0.0f;
    for (int i = 0; i < n; i++) {
        cv::Point2f p = points.at<cv::Point2f>(i);
        sum_x += p.x;
        sum_y += p.y;
    }
    int cx = static_cast<int>(sum_x / n);
    int cy = static_cast<int>(sum_y / n);
    cv::circle(img, cv::Point(cx, cy), 5, cv::Scalar(0, 255, 0), -1);

    return data;
}

namespace {

/**
 * @brief 3位数字字符串转 int 数组
 * @param s 长度为3的数字字符串
 * @return 3个整数的向量
 */
std::vector<int> extract_digits(const std::string& s) noexcept {
    return {s[0] - '0', s[1] - '0', s[2] - '0'};
}

/**
 * @brief 校验数字范围是否在 1~6
 * @param digits 数字向量
 * @return 全部合法返回 true
 */
bool is_valid_digit_range(const std::vector<int>& digits) noexcept {
    for (int d : digits) {
        if (d < 1 || d > 6) return false;
    }
    return true;
}

} // namespace

/**
 * @brief 解析二维码字符串为 TaskCode
 * @param qr_data 格式 "XXX+YYY+ZZZ+WWW"，4段3位数字
 * @return 解析结果；格式非法、非数字、数值越界返回 std::nullopt
 * @note 每位数字须在 1~6 范围内（颜色/位置编号）
 */
std::optional<TaskCode> TaskCodeParser::parse(const std::string& qr_data) {
    if (qr_data.empty())
        return std::nullopt;
    if (qr_data.find('+') == std::string::npos)
        return std::nullopt;

    // 按 '+' 分割为4段
    std::vector<std::string> parts;
    std::string current;
    for (char c : qr_data) {
        if (c == '+') {
            parts.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    parts.push_back(current);

    // 必须恰好4段
    if (parts.size() != 4)
        return std::nullopt;

    // 每段必须为3位数字
    for (const auto& p : parts) {
        if (p.length() != 3)
            return std::nullopt;
        for (char c : p) {
            if (!std::isdigit(static_cast<unsigned char>(c)))
                return std::nullopt;
        }
    }

    TaskCode code;
    code.batch1_colors = extract_digits(parts[0]);
    code.batch1_positions = extract_digits(parts[1]);
    code.batch2_colors = extract_digits(parts[2]);
    code.batch2_positions = extract_digits(parts[3]);

    // 校验数字范围 1~6
    if (!is_valid_digit_range(code.batch1_colors)) return std::nullopt;
    if (!is_valid_digit_range(code.batch2_colors)) return std::nullopt;
    if (!is_valid_digit_range(code.batch1_positions)) return std::nullopt;
    if (!is_valid_digit_range(code.batch2_positions)) return std::nullopt;

    return code;
}
