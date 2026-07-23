/**
 * @file task_display.hpp
 * @brief 任务码显示装置模块
 *
 * 将解析后的 TaskCode 渲染为可视化信息面板，
 * 包含原始码文本、批次颜色/位置说明和进度条。
 * 用于调试和现场确认任务码内容。
 */
#pragma once

#include "qr_detector.hpp"
#include <string>
#include <vector>
#include <opencv2/opencv.hpp>

/**
 * @brief 任务码显示渲染器
 *
 * 生成固定尺寸的 BGR 图像，展示任务码原文、
 * 两批次的颜色/位置说明以及完成进度条。
 */
class TaskDisplay {
public:
    /**
     * @brief 构造函数
     * @param width 显示面板宽度 (px)，默认 400
     * @param height 显示面板高度 (px)，默认 200
     */
    explicit TaskDisplay(int width = 400, int height = 200);

    /**
     * @brief 渲染任务码显示图
     * @param task_code 原始 QR 码字符串
     * @param completed_steps 各步骤完成状态，true 表示已完成
     * @return 渲染后的 BGR 图像
     */
    [[nodiscard]] cv::Mat render(const std::string& task_code,
                                const std::vector<bool>& completed_steps = {});

private:
    int width_;           ///< 面板宽度 (px)
    int height_;          ///< 面板高度 (px)
    TaskCodeParser parser_; ///< 任务码解析器
};
