/**
 * 任务码显示装置模块实现
 * 对应 Python: vision/task_display.py
 */
#include "task_display.hpp"
#include <unordered_map>

// 颜色编号 -> 中文名
static const std::unordered_map<int, std::string> COLOR_NAMES = {
    {1, "红"}, {2, "黄"}, {3, "蓝"},
    {4, "绿"}, {5, "黑"}, {6, "浅蓝"}
};

TaskDisplay::TaskDisplay(int width, int height)
    : m_width(width), m_height(height) {}

cv::Mat TaskDisplay::render(const std::string& taskCode,
                            const std::vector<bool>& completedSteps) {
    cv::Mat display(m_height, m_width, CV_8UC3, cv::Scalar(0, 0, 0));

    cv::putText(display, "TASK CODE", cv::Point(10, 25),
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 255), 2);

    if (!taskCode.empty()) {
        cv::putText(display, taskCode, cv::Point(10, 80),
                    cv::FONT_HERSHEY_SIMPLEX, 1.4, cv::Scalar(0, 255, 0), 3);

        auto parsed = TaskCodeParser::parse(taskCode);
        if (parsed) {
            const auto& b1c = parsed->batch1_colors;
            const auto& b1p = parsed->batch1_positions;
            const auto& b2c = parsed->batch2_colors;
            const auto& b2p = parsed->batch2_positions;

            auto getColorName = [](int id) -> std::string {
                auto it = COLOR_NAMES.find(id);
                return it != COLOR_NAMES.end() ? it->second : "?";
            };

            std::string info1 = "B1:" + getColorName(b1c[0]) + "/" +
                                getColorName(b1c[1]) + "/" +
                                getColorName(b1c[2]) + " -> R" +
                                std::to_string(b1p[0]) +
                                std::to_string(b1p[1]) +
                                std::to_string(b1p[2]);
            std::string info2 = "B2:" + getColorName(b2c[0]) + "/" +
                                getColorName(b2c[1]) + "/" +
                                getColorName(b2c[2]) + " -> R" +
                                std::to_string(b2p[0]) +
                                std::to_string(b2p[1]) +
                                std::to_string(b2p[2]);

            cv::putText(display, info1, cv::Point(10, 115),
                        cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255), 1);
            cv::putText(display, info2, cv::Point(10, 140),
                        cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255), 1);
        }
    }

    if (!completedSteps.empty()) {
        int progress = 0;
        for (bool done : completedSteps) if (done) progress++;
        int total = static_cast<int>(completedSteps.size());
        int pct = total > 0 ? progress * 100 / total : 0;

        cv::putText(display, "PROGRESS: " + std::to_string(progress) + "/" +
                    std::to_string(total) + " (" + std::to_string(pct) + "%)",
                    cv::Point(10, 175), cv::FONT_HERSHEY_SIMPLEX, 0.5,
                    cv::Scalar(0, 255, 255), 2);

        int barW = m_width - 20;
        int fillW = barW * pct / 100;
        cv::rectangle(display, cv::Point(10, 185), cv::Point(10 + barW, 195),
                      cv::Scalar(50, 50, 50), -1);
        cv::rectangle(display, cv::Point(10, 185), cv::Point(10 + fillW, 195),
                      cv::Scalar(0, 255, 0), -1);
    }

    return display;
}
