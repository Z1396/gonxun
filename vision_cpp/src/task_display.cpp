/**
 * @file task_display.cpp
 * @brief 任务码显示装置模块实现文件
 * 
 * @details 本文件实现了智能物流搬运系统的任务码可视化显示功能。
 *          核心功能：
 *          - 任务码显示：在图像上绘制任务码字符串
 *          - 任务码解析：解析任务码并显示批次信息、颜色配置
 *          - 进度条显示：实时显示任务完成进度
 *          - 颜色标识：将颜色编号转换为中文名称
 * 
 * @author 智能物流搬运系统开发团队
 * @version 1.0
 * @date 2025-01-01
 * 
 * @note 修改历史：
 *       - 2025-01-01: 初始版本，移植自 Python 版本 vision/task_display.py
 *       - 2025-02-20: 增加进度条显示功能
 * 
 * @note 任务码格式：
 *       任务码为12位字符串，包含两批次物料配置：
 *       - 前6位：批次1（3位颜色码 + 3位位置码）
 *       - 后6位：批次2（3位颜色码 + 3位位置码）
 *       - 示例："123456789ABC"
 *       
 * @see task_display.hpp
 */
#include "task_display.hpp"
#include <unordered_map>

/**
 * @brief 颜色编号到中文名称的映射表
 * 
 * @details 用于在任务码解析时将数字编号转换为易于理解的中文名称。
 *          映射关系：
 *          - 1: 红色
 *          - 2: 黄色
 *          - 3: 蓝色
 *          - 4: 绿色
 *          - 5: 黑色
 *          - 6: 浅蓝色
 */
static const std::unordered_map<int, std::string> COLOR_NAMES = {
    {1, "红"}, {2, "黄"}, {3, "蓝"},
    {4, "绿"}, {5, "黑"}, {6, "浅蓝"}
};

/**
 * @brief 构造函数，初始化显示区域尺寸
 * 
 * @param width 显示图像宽度（像素）
 * @param height 显示图像高度（像素）
 */
TaskDisplay::TaskDisplay(int width, int height)
    : m_width(width), m_height(height) {}

/**
 * @brief 渲染任务码显示图像
 * 
 * @details 在黑色背景上绘制任务码、批次信息和进度条。
 *          任务码解析后显示两批次的颜色配置和位置信息。
 * 
 * @param taskCode 任务码字符串（12位）
 * @param completedSteps 已完成步骤的布尔列表
 *        - true: 该步骤已完成
 *        - false: 该步骤未完成
 * 
 * @return cv::Mat 渲染后的显示图像（BGR格式）
 * 
 * @note 显示内容：
 *       - 顶部："TASK CODE" 标题（黄色）
 *       - 中部：任务码字符串（大号绿色）
 *       - 下部：批次信息（B1: 红/黄/蓝 → R123, B2: 绿/黑/浅蓝 → R456）
 *       - 底部：进度条和百分比（已完成/总数）
 *       
 * @note 进度条绘制：
 *       - 背景条：深灰色矩形
 *       - 进度条：绿色填充矩形，宽度根据完成百分比计算
 *       - 文字："PROGRESS: X/Y (Z%)"
 *       
 * @see TaskCodeParser::parse()
 */
cv::Mat TaskDisplay::render(const std::string& taskCode,
                            const std::vector<bool>& completedSteps) {
    // 创建黑色背景图像
    cv::Mat display(m_height, m_width, CV_8UC3, cv::Scalar(0, 0, 0));

    // 绘制标题："TASK CODE"（黄色）
    cv::putText(display, "TASK CODE", cv::Point(10, 25),
                cv::FONT_HERSHEY_SIMPLEX, 0.6, cv::Scalar(0, 255, 255), 2);

    // 如果任务码非空，绘制任务码和批次信息
    if (!taskCode.empty()) {
        // 绘制任务码字符串（大号绿色）
        cv::putText(display, taskCode, cv::Point(10, 80),
                    cv::FONT_HERSHEY_SIMPLEX, 1.4, cv::Scalar(0, 255, 0), 3);

        // 解析任务码
        auto parsed = TaskCodeParser::parse(taskCode);
        if (parsed) {
            // 提取批次1和批次2的配置
            const auto& b1c = parsed->batch1_colors;      // 批次1颜色
            const auto& b1p = parsed->batch1_positions;   // 批次1位置
            const auto& b2c = parsed->batch2_colors;      // 批次2颜色
            const auto& b2p = parsed->batch2_positions;   // 批次2位置

            // 颜色编号转中文名称的lambda函数
            auto getColorName = [](int id) -> std::string {
                auto it = COLOR_NAMES.find(id);
                return it != COLOR_NAMES.end() ? it->second : "?";
            };

            // 构建批次1信息字符串
            // 格式："B1:红/黄/蓝 -> R123"
            std::string info1 = "B1:" + getColorName(b1c[0]) + "/" +
                                getColorName(b1c[1]) + "/" +
                                getColorName(b1c[2]) + " -> R" +
                                std::to_string(b1p[0]) +
                                std::to_string(b1p[1]) +
                                std::to_string(b1p[2]);
            
            // 构建批次2信息字符串
            // 格式："B2:绿/黑/浅蓝 -> R456"
            std::string info2 = "B2:" + getColorName(b2c[0]) + "/" +
                                getColorName(b2c[1]) + "/" +
                                getColorName(b2c[2]) + " -> R" +
                                std::to_string(b2p[0]) +
                                std::to_string(b2p[1]) +
                                std::to_string(b2p[2]);

            // 绘制批次信息（白色）
            cv::putText(display, info1, cv::Point(10, 115),
                        cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255), 1);
            cv::putText(display, info2, cv::Point(10, 140),
                        cv::FONT_HERSHEY_SIMPLEX, 0.4, cv::Scalar(255, 255, 255), 1);
        }
    }

    // 如果有进度信息，绘制进度条
    if (!completedSteps.empty()) {
        // 计算已完成步骤数
        int progress = 0;
        for (bool done : completedSteps) if (done) progress++;
        
        // 计算总步骤数和完成百分比
        int total = static_cast<int>(completedSteps.size());
        int pct = total > 0 ? progress * 100 / total : 0;

        // 绘制进度文字（黄色）
        cv::putText(display, "PROGRESS: " + std::to_string(progress) + "/" +
                    std::to_string(total) + " (" + std::to_string(pct) + "%)",
                    cv::Point(10, 175), cv::FONT_HERSHEY_SIMPLEX, 0.5,
                    cv::Scalar(0, 255, 255), 2);

        // 计算进度条尺寸
        int barW = m_width - 20;        // 进度条总宽度
        int fillW = barW * pct / 100;   // 进度条填充宽度
        
        // 绘制进度条背景（深灰色）
        cv::rectangle(display, cv::Point(10, 185), cv::Point(10 + barW, 195),
                      cv::Scalar(50, 50, 50), -1);
        
        // 绘制进度条填充（绿色）
        cv::rectangle(display, cv::Point(10, 185), cv::Point(10 + fillW, 195),
                      cv::Scalar(0, 255, 0), -1);
    }

    return display;
}
