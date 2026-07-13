/**
 * 任务码显示装置模块
 * 对应 Python: vision/task_display.py
 * 比赛规则：搬运机器人必须配备任务码显示装置
 */
#pragma once

#include "qr_detector.hpp"
#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

class TaskDisplay {
public:
    TaskDisplay(int width = 400, int height = 200);

    /** 渲染任务码显示图 */
    cv::Mat render(const std::string& taskCode,
                   const std::vector<bool>& completedSteps = {});

private:
    int m_width;
    int m_height;
    TaskCodeParser m_parser;
};
