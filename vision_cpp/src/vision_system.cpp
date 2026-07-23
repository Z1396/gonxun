/**
 * @file vision_system.cpp
 * @brief 视觉系统核心调度模块实现
 *
 * 系统顶层设计：
 *  1. 统一初始化所有视觉子模块（串口、双相机、YOLO检测、卡尔曼滤波、圆环/二维码检测器）
 *  2. 单帧统一入口 process_frame，根据下位机工作模式自动分发任务
 *  3. 所有检测流程统一遵循：目标识别 -> 合法性校验 -> 卡尔曼平滑 -> 画面渲染 -> 串口上报
 *  4. 全覆盖异常捕获，单模式报错不影响整体视觉线程运行
 *
 * 工作模式分发逻辑：
 *  - MODE_COLOR: 检测红/绿/蓝三色物料 → 滤波 → 发送坐标
 *  - MODE_RING:  检测3个定位圆环 → 滤波 → 发送坐标
 *  - MODE_DOCK:  检测蓝/绿/红三色 → 滤波 → 发送坐标（对接停靠）
 *  - MODE_QR:    读取扫码摄像头 → 解码 → 发送 QR 数据
 */
#include "vision_system.hpp"
#include "config.hpp"

#include <climits>
#include <iostream>

/**
 * @brief VisionSystem 构造函数
 * @param serial_mock 是否开启串口模拟模式（调试无硬件时使用）
 * @param serial_port 自定义串口设备路径，为空则读取配置文件默认值
 * @param baudrate 串口波特率
 * @param main_camera 主摄像头设备索引，-1 使用默认配置
 * @param qr_camera 扫码摄像头设备索引，-1 使用默认配置
 *
 * 初始化模块清单：
 *  1. 串口通信模块：负责向下位机上报视觉坐标、二维码数据
 *  2. 双相机采集模块：主相机（物料/圆环）+ 扫码相机（二维码）
 *  3. YOLO检测器：智能颜色目标识别
 *  4. 三组卡尔曼滤波器：分别平滑三组目标坐标，消除抖动噪点
 *
 * @note 完全基于 config 配置文件参数初始化，统一项目参数管理
 * @note 启动自检，提示YOLO模型加载状态，提前暴露初始化异常
 */
VisionSystem::VisionSystem(bool serial_mock, const std::string& serial_port,
                           int baudrate, int main_camera, int qr_camera)
    // 初始化串口通信：自定义参数兜底，无参数使用全局配置
    : serial_comm(serial_mock,
                  serial_port.empty() ? config::SERIAL_PORT : serial_port,
                  baudrate,
                  MODE_IDLE,
                  config::SERIAL_MOCK_CYCLE),
    // 初始化双相机：分辨率、设备索引全部读取配置文件
      camera(main_camera >= 0 ? main_camera : config::CAMERA_MAIN_INDEX,
             qr_camera >= 0 ? qr_camera : config::CAMERA_QR_INDEX,
             config::CAMERA_MAIN_WIDTH, config::CAMERA_MAIN_HEIGHT,
             config::CAMERA_QR_WIDTH, config::CAMERA_QR_HEIGHT),
    // 初始化YOLO颜色检测模型：模型路径、输入尺寸、置信度阈值取自配置
      yolo_detector_(config::YOLO_TS_MODEL_PATH,
                     config::YOLO_IMGSZ,
                     config::YOLO_CONF_THRESHOLD),
    // 初始化三组卡尔曼滤波器，分别对应三组检测目标
      kalman_filters_{
          KalmanFilter(config::KALMAN_Q, config::KALMAN_R),
          KalmanFilter(config::KALMAN_Q, config::KALMAN_R),
          KalmanFilter(config::KALMAN_Q, config::KALMAN_R)
      } {

    std::cout << "VisionSystem 初始化完成" << std::endl;

    // 模型加载自检，提前预警检测功能异常
    if (!yolo_detector_.is_available()) {
        std::cerr << "[警告] YOLO 模型未加载，颜色检测功能不可用" << std::endl;
    }
}

/// @brief 设置当前任务码，用于动态颜色检测
void VisionSystem::set_task_code(const TaskCode& task_code) {
    current_task_ = task_code;
    task_set_ = true;
}

/// @brief 设置当前批次 (1 或 2)
void VisionSystem::set_current_batch(int batch) {
    current_batch_ = (batch == 1 || batch == 2) ? batch : 1;
}

/**
 * @brief 单目标坐标卡尔曼滤波平滑函数
 * @param x 原始检测X坐标
 * @param y 原始检测Y坐标
 * @param kf_index 滤波器索引 0~2，对应三组独立目标
 * @return std::pair<int, int> 整型平滑后坐标
 *
 * 核心作用：
 *  1. 抑制相机抖动、识别噪点导致的坐标跳变
 *  2. 三组目标独立滤波，互不干扰
 *  3. 浮点滤波计算 + 整型输出，适配下位机指令格式
 */
std::pair<int, int> VisionSystem::filter_position(float x, float y, int kf_index) {
    // 构造二维观测矩阵 (X,Y)
    cv::Matx21f z(x, y);
    // 卡尔曼迭代滤波，输出平滑后坐标
    auto filtered = kalman_filters_[kf_index].filter(z);
    // 转为整型坐标，适配串口协议
    return {static_cast<int>(filtered(0)), static_cast<int>(filtered(1))};
}

/**
 * @brief 通用三色目标检测、滤波、渲染工具函数（高度复用）
 * @param img 输入原图，函数内部直接绘制标注
 * @param color_specs 颜色配置数组：(颜色识别名, 显示标签, BGR绘制颜色)
 * @param min_area 目标最小面积，过滤噪点小色块
 * @param max_area 目标最大面积，过滤超大干扰区域
 * @return std::vector<std::pair<int, int>> 三组滤波后坐标，检测失败返回空数组
 *
 * 核心逻辑：
 *  1. 遍历配置的三种颜色，逐一检测目标中心
 *  2. 任意一个颜色检测缺失，直接返回空（保证三组目标完整才上报）
 *  3. 对三组坐标分别独立卡尔曼平滑
 *  4. 在原图绘制中心点+文字标签，用于UI可视化预览
 *
 * @note 强一致性校验：必须同时识别到三个目标，才执行后续上报，避免单目标异常
 */
std::vector<std::pair<int, int>> VisionSystem::detect_three_colors(
    cv::Mat& img,
    const std::vector<std::tuple<std::string, std::string, cv::Scalar>>& color_specs,
    int min_area, int max_area) {

    std::vector<std::pair<int, int>> positions;

    // 逐一检测指定颜色目标
    for (const auto& [color, label, draw_color] : color_specs) {
        // YOLO检测目标中心，带面积阈值过滤
        auto pos = yolo_detector_.detect_center(img, color, min_area, max_area);
        // 任一目标缺失，直接返回空，放弃本次帧上报
        if (!pos) return {};
        positions.push_back({pos->x, pos->y});
    }

    // 对三组原始坐标分别滤波平滑
    std::vector<std::pair<int, int>> filtered;
    for (size_t idx = 0; idx < positions.size(); ++idx) {
        auto [fx, fy] = filter_position(
            static_cast<float>(positions[idx].first),
            static_cast<float>(positions[idx].second),
            static_cast<int>(idx));
        filtered.push_back({fx, fy});

        // 画面可视化渲染：实心圆点 + 字符标签
        const auto& [color, label, draw_color] = color_specs[idx];
        cv::circle(img, cv::Point(fx, fy), 8, draw_color, -1);
        cv::putText(img, label, cv::Point(fx - 5, fy - 15),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, draw_color, 1);
    }

    return filtered;
}

/**
 * @brief 物料颜色检测模式处理流程 MODE_COLOR
 * @param result_img 待渲染图像
 * @details
 *  根据任务码动态选择3种颜色进行检测
 *  流程：检测 -> 滤波平滑 -> 画面绘制 -> 串口上报 CMD_COLOR
 *  异常防护：try-catch捕获单帧异常，不崩溃线程
 */
void VisionSystem::process_color(cv::Mat& result_img) {
    try {
        std::vector<std::tuple<std::string, std::string, cv::Scalar>> color_specs;

        // 根据任务码获取颜色序列
        if (task_set_) {
            // 获取当前批次的颜色编号
            const auto& color_codes = (current_batch_ == 1)
                ? current_task_.batch1_colors
                : current_task_.batch2_colors;

            // 构建颜色检测规格
            for (int code : color_codes) {
                color_specs.emplace_back(
                    color_code_to_name(code),
                    color_code_to_label(code),
                    color_code_to_bgr(code)
                );
            }
        } else {
            // 默认检测红、黄、蓝（作为备选）
            color_specs = {
                {"red",    "R", cv::Scalar(0, 0, 255)},
                {"yellow", "Y", cv::Scalar(0, 255, 255)},
                {"blue",   "B", cv::Scalar(255, 0, 0)}
            };
        }

        // 统一调用通用三色检测模板
        auto filtered = detect_three_colors(result_img, color_specs, 2000, 10000);

        // 检测完整则通过串口上报坐标
        if (!filtered.empty()) {
            serial_comm.send_coordinates(CMD_COLOR, filtered);
        }
    } catch (const std::exception& e) {
        std::cerr << "unit=1处理异常: " << e.what() << std::endl;
    }
}

/**
 * @brief 圆环定位检测模式处理流程 MODE_RING
 * @param result_img 待渲染图像
 * @details
 *  专属三环定位算法检测三个定位圆环
 *  三组坐标独立卡尔曼滤波
 *  绘制青蓝色标记点，标注L/M/R左右中位置
 *  完整检测后串口上报 CMD_RING 坐标数据
 */
void VisionSystem::process_ring(cv::Mat& result_img) {
    try {
        // 调用专用圆环检测器获取三组坐标
        auto circle_pos = three_ring_detector_.detect(result_img);
        if (!circle_pos) return;

        // 对左、中、右三个圆环坐标分别滤波
        std::vector<std::pair<int, int>> filtered;
        for (int idx = 0; idx < 3; ++idx) {
            auto [fx, fy] = filter_position(
                static_cast<float>((*circle_pos)[static_cast<size_t>(idx) * 2]),
                static_cast<float>((*circle_pos)[idx * 2 + 1]),
                idx);
            filtered.push_back({fx, fy});
        }

        // 可视化绘制圆环定位点与标签
        const char* labels[] = {"L", "M", "R"};
        for (int i = 0; i < 3; ++i) {
            cv::circle(result_img, cv::Point(filtered[i].first, filtered[i].second),
                       8, cv::Scalar(0, 255, 255), -1);
            cv::putText(result_img, labels[i],
                        cv::Point(filtered[i].first - 5, filtered[i].second - 15),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 255), 1);
        }

        // 上报圆环定位坐标
        serial_comm.send_coordinates(CMD_RING, filtered);
    } catch (const std::exception& e) {
        std::cerr << "unit=3处理异常: " << e.what() << std::endl;
    }
}

/**
 * @brief 停靠对接检测模式处理流程 MODE_DOCK
 * @param result_img 待渲染图像
 * @details
 *  检测顺序：蓝、绿、红三色停靠标识
 *  复用通用三色检测模板，调整面积阈值适配停靠目标尺寸
 *  滤波平滑后上报 CMD_DOCK 坐标，用于机器人精准停靠
 */
void VisionSystem::process_dock(cv::Mat& result_img) {
    try {
        // 停靠模式三色检测规则
        std::vector<std::tuple<std::string, std::string, cv::Scalar>> color_specs = {
            {"blue",  "B", cv::Scalar(255, 0, 0)},
            {"green", "G", cv::Scalar(0, 255, 0)},
            {"red",   "R", cv::Scalar(0, 0, 255)}
        };

        // 停靠目标面积更大，调整面积阈值
        auto filtered = detect_three_colors(result_img, color_specs, 3000, 10000);

        if (!filtered.empty()) {
            serial_comm.send_coordinates(CMD_DOCK, filtered);
        }
    } catch (const std::exception& e) {
        std::cerr << "unit=4处理异常: " << e.what() << std::endl;
    }
}

/**
 * @brief 二维码识别模式处理流程 MODE_QR
 * @param result_img 主相机图像，用于画面渲染
 * @details 双相机降级容错机制：
 *  1. 优先读取专用扫码摄像头图像解码
 *  2. 扫码相机异常/无设备时，降级使用主相机图像解码
 *  3. 解码成功后打印日志、渲染文本、串口上报二维码字符串
 */
void VisionSystem::process_qr(cv::Mat& result_img) {
    try {
        // 优先读取扫码摄像头
        auto [success, qr_img] = camera.read_qr();

        // 双相机容错降级：扫码相机失败则使用主相机画面
        cv::Mat target_img = success ? qr_img : result_img;
        if (target_img.empty()) return;

        // 二维码解码
        auto qr_data = qr_detector.detect(target_img);

        if (qr_data) {
            std::cout << "二维码识别成功: " << *qr_data << std::endl;

            // 在主画面左上角渲染二维码数据
            cv::putText(result_img, "QR: " + *qr_data, cv::Point(10, 30),
                        cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);

            // 串口上报二维码字符串
            serial_comm.send_qr_data(*qr_data);
        }
    } catch (const std::exception& e) {
        std::cerr << "unit=9处理异常: " << e.what() << std::endl;
    }
}

/**
 * @brief 视觉系统单帧处理统一入口（顶层调度函数）
 * @param img 原始相机BGR图像
 * @param unit 手动指定工作模式，-1 自动从串口原子变量读取下位机下发模式
 * @return cv::Mat 绘制完成、带模式标注的可视化结果图像
 *
 * 执行流程：
 *  1. 空帧校验，无效图像直接返回
 *  2. 自动/手动获取当前工作模式
 *  3. 根据模式分支分发至对应处理函数
 *  4. 画面底部渲染当前工作模式水印
 *  5. 返回可视化图像供UI显示
 *
 * @note 线程安全：所有模式分支独立，异常隔离，单帧报错不阻塞后续帧
 */
cv::Mat VisionSystem::process_frame(const cv::Mat& img, int unit) 
{
    // 空图像防护
    if (img.empty()) return cv::Mat();

    // 自动模式：读取串口模块原子变量（下位机实时下发的工作指令）
    if (unit < 0) 
    {
        unit = serial_comm.unit.load();
    }

    // 克隆原图，避免修改原始相机数据
    cv::Mat result_img = img.clone();

    // 多模式任务分发
    switch (unit) {
        case MODE_COLOR: process_color(result_img); break;
        case MODE_RING:  process_ring(result_img);  break;
        case MODE_DOCK:  process_dock(result_img);  break;
        case MODE_QR:    process_qr(result_img);    break;
        default: break;
    }

    // 底部状态栏绘制当前工作模式
    const char* mode_text = "UNK";
    switch (unit) {
        case MODE_IDLE:  mode_text = "IDLE";  break;
        case MODE_COLOR: mode_text = "COLOR"; break;
        case MODE_RING:  mode_text = "RING";  break;
        case MODE_DOCK:  mode_text = "DOCK";  break;
        case MODE_QR:    mode_text = "QR";    break;
        default: break;
    }

    cv::putText(result_img, std::string("Mode: ") + mode_text,
                cv::Point(10, result_img.rows - 10),
                cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 255), 2);

    return result_img;
}
