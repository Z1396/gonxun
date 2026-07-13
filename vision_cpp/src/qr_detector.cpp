#include "qr_detector.hpp"

#include <cctype>

QRDetector::QRDetector() = default;

std::optional<std::string> QRDetector::detect(cv::Mat& img) {
    if (img.empty()) return std::nullopt;
    cv::Mat points;
    std::string data = _decoder.detectAndDecode(img, points);
    if (data.empty()) return std::nullopt;
    return data;
}

std::optional<std::string> QRDetector::detectAndDraw(cv::Mat& img) {
    if (img.empty()) return std::nullopt;
    cv::Mat points;
    std::string data = _decoder.detectAndDecode(img, points);
    if (data.empty() || points.empty()) return std::nullopt;

    int n = static_cast<int>(points.total());
    // 绘制边框: 依次连接相邻顶点, 末尾连回首点
    for (int j = 0; j < n; j++) {
        cv::Point2f p1 = points.at<cv::Point2f>(j);
        cv::Point2f p2 = points.at<cv::Point2f>((j + 1) % n);
        cv::line(img,
                 cv::Point(static_cast<int>(p1.x), static_cast<int>(p1.y)),
                 cv::Point(static_cast<int>(p2.x), static_cast<int>(p2.y)),
                 cv::Scalar(255, 0, 0), 3);
    }

    // 计算并绘制中心点
    float sumX = 0.0f, sumY = 0.0f;
    for (int i = 0; i < n; i++) {
        cv::Point2f p = points.at<cv::Point2f>(i);
        sumX += p.x;
        sumY += p.y;
    }
    int cx = static_cast<int>(sumX / n);
    int cy = static_cast<int>(sumY / n);
    cv::circle(img, cv::Point(cx, cy), 5, cv::Scalar(0, 255, 0), -1);

    return data;
}

std::optional<TaskCode> TaskCodeParser::parse(const std::string& qr_data) {
    if (qr_data.empty()) return std::nullopt;
    if (qr_data.find('+') == std::string::npos) return std::nullopt;

    // 按 '+' 分割字符串
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
    if (parts.size() != 4) return std::nullopt;

    // 每段必须是3位纯数字
    for (const auto& p : parts) {
        if (p.length() != 3) return std::nullopt;
        for (char c : p) {
            if (!std::isdigit(static_cast<unsigned char>(c))) return std::nullopt;
        }
    }

    // 提取数字 (字符转int)
    auto extractDigits = [](const std::string& s) {
        return std::vector<int>{s[0] - '0', s[1] - '0', s[2] - '0'};
    };

    TaskCode code;
    code.batch1_colors = extractDigits(parts[0]);
    code.batch1_positions = extractDigits(parts[1]);
    code.batch2_colors = extractDigits(parts[2]);
    code.batch2_positions = extractDigits(parts[3]);

    // 校验颜色编号范围 (1-6)
    for (int c : code.batch1_colors) {
        if (c < 1 || c > 6) return std::nullopt;
    }
    for (int c : code.batch2_colors) {
        if (c < 1 || c > 6) return std::nullopt;
    }
    // 校验投放位置范围 (1-6)
    for (int r : code.batch1_positions) {
        if (r < 1 || r > 6) return std::nullopt;
    }
    for (int r : code.batch2_positions) {
        if (r < 1 || r > 6) return std::nullopt;
    }

    return code;
}
