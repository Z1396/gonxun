/**
 * @file task_display.cpp
 * @brief 任务码显示装置模块实现
 *
 * 将 TaskCode 解析结果渲染为信息面板图像，包含:
 * - "TASK CODE" 标题
 * - 原始码字符串
 * - 两批次的颜色名/投放位置说明
 * - 进度条（基于 completed_steps）
 */
#include "task_display.hpp"

#include <unordered_map>

namespace {

/// 颜色编号 → 中文名称映射 (1=红, 2=黄, 3=蓝, 4=绿, 5=黑, 6=浅蓝)
const std::unordered_map<int, std::string> COLOR_NAMES = {
    {1, "红"}, {2, "黄"}, {3, "蓝"},
    {4, "绿"}, {5, "黑"}, {6, "浅蓝"}
};

/**
 * @brief 获取颜色编号对应的中文名
 * @param id 颜色编号 (1-6)
 * @return 中文名称；无效编号返回 "?"
 */
std::string get_color_name(int id) noexcept {
    auto it = COLOR_NAMES.find(id);
    return it != COLOR_NAMES.end() ? it->second : "?";
}

} // namespace

/**
 * @brief 构造函数
 * @param width 面板宽度 (px)
 * @param height 面板高度 (px)
 */
TaskDisplay::TaskDisplay(int width, int height)
    : width_(width), height_(height) {}

/**
 * @brief 渲染任务码显示图
 * @param task_code 原始 QR 码字符串
 * @param completed_steps 各步骤完成状态
 * @return 渲染后的 BGR 图像
 * @note 布局: 标题(y=25) → 原始码(y=80) → 批次说明(y=115,140) → 进度条(y=175,185-195)
 */
cv::Mat TaskDisplay::render(const std::string& task_code,
                            const std::vector<bool>& completed_steps) {
    cv::Mat display(height_, width_, CV_8UC3, cv::Scalar(0, 0, 0));

    // 标题
    cv::putText(display, "TASK CODE", cv::Point(10, 25),
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 255), 2);

    if (!task_code.empty()) {
        // 原始码文本
        cv::putText(display, task_code, cv::Point(10, 80),
                    cv::FONT_HERSHEY_SIMPLEX, 1.4, cv::Scalar(0, 255, 0), 3);

        // 解析并显示批次说明
        auto parsed = TaskCodeParser::parse(task_code);
        if (parsed) {
            const auto& b1c = parsed->batch1_colors;
            const auto& b1p = parsed->batch1_positions;
            const auto& b2c = parsed->batch2_colors;
            const auto& b2p = parsed->batch2_positions;

            // 格式: "B1:红/绿/蓝 -> R123"
            std::string info1 = "B1:" + get_color_name(b1c[0]) + "/" +
                                get_color_name(b1c[1]) + "/" +
                                get_color_name(b1c[2]) + " -> R" +
                                std::to_string(b1p[0]) +
                                std::to_string(b1p[1]) +
                                std::to_string(b1p[2]);

            std::string info2 = "B2:" + get_color_name(b2c[0]) + "/" +
                                get_color_name(b2c[1]) + "/" +
                                get_color_name(b2c[2]) + " -> R" +
                                std::to_string(b2p[0]) +
                                std::to_string(b2p[1]) +
                                std::to_string(b2p[2]);

            cv::putText(display, info1, cv::Point(10, 115),
                        cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255), 1);
            cv::putText(display, info2, cv::Point(10, 140),
                        cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255), 1);
        }
    }

    // 进度条
    if (!completed_steps.empty()) {
        int progress = 0;
        for (bool done : completed_steps) if (done) progress++;

        int total = static_cast<int>(completed_steps.size());
        int pct = total > 0 ? progress * 100 / total : 0;

        cv::putText(display, "PROGRESS: " + std::to_string(progress) + "/" +
                    std::to_string(total) + " (" + std::to_string(pct) + "%)",
                    cv::Point(10, 175), cv::FONT_HERSHEY_SIMPLEX, 0.5,
                    cv::Scalar(0, 255, 255), 2);

        int bar_w = width_ - 20;
        int fill_w = bar_w * pct / 100;

        // 底色条
        cv::rectangle(display, cv::Point(10, 185), cv::Point(10 + bar_w, 195),
                      cv::Scalar(50, 50, 50), -1);

        // 填充条
        cv::rectangle(display, cv::Point(10, 185), cv::Point(10 + fill_w, 195),
                      cv::Scalar(0, 255, 0), -1);
    }

    return display;
}
