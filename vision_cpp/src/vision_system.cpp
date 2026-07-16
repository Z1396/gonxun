/**
 * @file vision_system.cpp
 * @brief 视觉系统核心调度模块实现文件
 * 
 * @details 本文件实现了智能物流搬运系统的视觉系统核心调度功能。
 *          核心功能：
 *          - 模块集成：串口通信、摄像头管理、YOLO检测、颜色检测、二维码识别
 *          - 模式分发：根据工作模式调用不同的处理函数
 *          - 坐标滤波：使用卡尔曼滤波平滑坐标数据
 *          - 数据可视化：在图像上绘制检测结果
 * 
 * @author 智能物流搬运系统开发团队
 * @version 1.0
 * @date 2025-01-01
 * 
 * @note 修改历史：
 *       - 2025-01-01: 初始版本，移植自 Python 版本 vision/system.py
 *       - 2025-02-28: 增加多颜色检测和卡尔曼滤波功能
 *       - 2025-03-25: 优化模式分发逻辑，增加异常处理
 * 
 * @note 工作模式：
 *       - MODE_COLOR (1): 三色物料检测
 *       - MODE_RING (3): 圆环检测
 *       - MODE_DOCK (4): 停靠点检测
 *       - MODE_QR (9): 二维码识别
 *       - MODE_IDLE (0): 空闲状态
 *       
 * @see vision_system.hpp
 */
#include "vision_system.hpp"
#include "config.hpp"
#include <iostream>
#include <climits>

/**
 * @brief 构造函数，初始化视觉系统的所有模块
 * 
 * @details 初始化串口通信、摄像头管理、YOLO检测器、卡尔曼滤波器等。
 *          如果 YOLO 模型加载失败，会输出警告但仍继续运行。
 * 
 * @param serialMock 是否启用串口模拟模式
 * @param serialPort 串口设备路径（如 "/dev/ttyUSB0"）
 * @param baudrate 串口波特率
 * @param mainCamera 主摄像头索引（用于目标检测）
 * @param qrCamera 扫码摄像头索引（用于二维码识别）
 * 
 * @note 模块初始化顺序：
 *       1. 串口通信模块（SerialComm）
 *       2. 摄像头管理模块（CameraManager）
 *       3. YOLO 检测器（YOLOv8Detector）
 *       4. 卡尔曼滤波器（KalmanFilter，共3个）
 *       
 * @note 参数默认值：
 *       - serialPort: 从 config::SERIAL_PORT 读取
 *       - mainCamera: 从 config::CAMERA_MAIN_INDEX 读取
 *       - qrCamera: 从 config::CAMERA_QR_INDEX 读取
 */
VisionSystem::VisionSystem(bool serialMock, const std::string& serialPort,
                           int baudrate, int mainCamera, int qrCamera)
    // 初始化串口通信模块
    : serialComm(serialMock,
                 serialPort.empty() ? config::SERIAL_PORT : serialPort,  // 使用默认路径或指定路径
                 baudrate,
                 MODE_IDLE,                    // 初始工作模式为空闲
                 config::SERIAL_MOCK_CYCLE),   // 是否循环测试
      // 初始化摄像头管理模块
      camera(mainCamera >= 0 ? mainCamera : config::CAMERA_MAIN_INDEX,  // 主摄像头索引
             qrCamera >= 0 ? qrCamera : config::CAMERA_QR_INDEX,        // 扫码摄像头索引
             config::CAMERA_MAIN_WIDTH, config::CAMERA_MAIN_HEIGHT,    // 主摄像头分辨率
             config::CAMERA_QR_WIDTH, config::CAMERA_QR_HEIGHT),       // 扫码摄像头分辨率
      // 初始化 YOLO 检测器
      m_yoloDetector(config::YOLO_TS_MODEL_PATH,       // TensorRT 模型路径
                     config::YOLO_IMGSZ,              // 输入图像尺寸
                     config::YOLO_CONF_THRESHOLD),   // 置信度阈值
      // 初始化卡尔曼滤波器（共3个，用于三色坐标滤波）
      m_kalmanFilters{
          KalmanFilter(config::KALMAN_Q, config::KALMAN_R),  // 第1个卡尔曼滤波器
          KalmanFilter(config::KALMAN_Q, config::KALMAN_R),  // 第2个卡尔曼滤波器
          KalmanFilter(config::KALMAN_Q, config::KALMAN_R)   // 第3个卡尔曼滤波器
      } {

    std::cout << "VisionSystem 初始化完成" << std::endl;
    
    // 检查 YOLO 模型是否加载成功
    if (!m_yoloDetector.isAvailable()) {
        std::cerr << "[警告] YOLO 模型未加载，颜色检测功能不可用" << std::endl;
    }
}

/**
 * @brief 使用卡尔曼滤波平滑坐标
 * 
 * @details 对检测到的原始坐标进行卡尔曼滤波，减少噪声和抖动。
 *          每种颜色使用独立的卡尔曼滤波器。
 * 
 * @param x X坐标（像素）
 * @param y Y坐标（像素）
 * @param kfIndex 卡尔曼滤波器索引（0, 1, 2）
 * 
 * @return std::pair<int, int> 滤波后的坐标 (x, y)
 * 
 * @note 卡尔曼滤波参数：
 *       - Q: 过程噪声协方差（控制平滑度）
 *       - R: 测量噪声协方差（控制响应速度）
 *       
 * @see KalmanFilter::filter()
 */
std::pair<int, int> VisionSystem::filterPosition(float x, float y, int kfIndex) {
    // 构建观测向量 [x, y]
    cv::Matx21f z(x, y);
    
    // 进行卡尔曼滤波
    auto filtered = m_kalmanFilters[kfIndex].filter(z);
    
    // 返回滤波后的整数坐标
    return {static_cast<int>(filtered(0)), static_cast<int>(filtered(1))};
}

/**
 * @brief 检测三种颜色的目标中心点
 * 
 * @details 对三种颜色分别进行检测，如果任意颜色缺失则返回空。
 *          检测结果经过卡尔曼滤波后绘制在图像上。
 * 
 * @param img 输入图像（会被修改，绘制检测结果）
 * @param colorSpecs 颜色配置列表
 *        - 格式：<颜色名, 标签, 绘制颜色(BGR)>
 *        - 示例：{"red", "R", cv::Scalar(0, 0, 255)}
 * @param minArea 最小检测面积（像素）
 * @param maxArea 最大检测面积（像素）
 * 
 * @return std::vector<std::pair<int, int>> 检测到的三个中心点坐标
 *         - 成功：返回3个坐标
 *         - 失败：返回空向量
 * 
 * @note 检测流程：
 *       1. 对每种颜色调用 YOLO 检测器
 *       2. 如果任意颜色未检测到，立即返回空
 *       3. 对三个坐标分别进行卡尔曼滤波
 *       4. 在图像上绘制检测结果（圆圈 + 标签）
 *       
 * @see YOLOv8Detector::detectCenter(), filterPosition()
 */
std::vector<std::pair<int, int>> VisionSystem::detectThreeColors(
    cv::Mat& img,
    const std::vector<std::tuple<std::string, std::string, cv::Scalar>>& colorSpecs,
    int minArea, int maxArea) {

    // 存储原始检测坐标
    std::vector<std::pair<int, int>> positions;
    
    // 步骤 1: 对每种颜色进行检测
    for (const auto& [color, label, drawColor] : colorSpecs) {
        auto pos = m_yoloDetector.detectCenter(img, color, minArea, maxArea);
        if (!pos) return {};  // 任意颜色缺失，返回空
        positions.push_back({pos->x, pos->y});
    }

    // 步骤 2: 卡尔曼滤波 + 绘制
    std::vector<std::pair<int, int>> filtered;
    for (size_t idx = 0; idx < positions.size(); ++idx) {
        // 对坐标进行卡尔曼滤波
        auto [fx, fy] = filterPosition(
            static_cast<float>(positions[idx].first),
            static_cast<float>(positions[idx].second),
            static_cast<int>(idx));
        filtered.push_back({fx, fy});

        // 在图像上绘制检测结果
        const auto& [color, label, drawColor] = colorSpecs[idx];
        cv::circle(img, cv::Point(fx, fy), 8, drawColor, -1);  // 绘制圆圈
        cv::putText(img, label, cv::Point(fx - 5, fy - 15),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, drawColor, 1);  // 绘制标签
    }
    
    return filtered;
}

/**
 * @brief 处理三色物料检测模式（MODE_COLOR）
 * 
 * @details 检测红、绿、蓝三种颜色的物料中心点，并通过串口发送坐标。
 *          
 * @param resultImg 结果图像（会被修改，绘制检测结果）
 * 
 * @note 检测配置：
 *       - 红色：标签 "R"，绘制颜色 (0, 0, 255)
 *       - 绿色：标签 "G"，绘制颜色 (0, 255, 0)
 *       - 蓝色：标签 "B"，绘制颜色 (255, 0, 0)
 *       - 面积范围：2000-10000 像素
 *       
 * @note 发送数据：
 *       - 命令：CMD_COLOR
 *       - 数据：三个坐标 (X1, Y1, X2, Y2, X3, Y3)
 *       
 * @see detectThreeColors(), SerialComm::sendCoordinates()
 */
void VisionSystem::processColor(cv::Mat& resultImg) {
    try {
        // 三色识别配置: 颜色名, 标签, 绘制颜色(BGR)
        std::vector<std::tuple<std::string, std::string, cv::Scalar>> colorSpecs = {
            {"red",   "R", cv::Scalar(0, 0, 255)},    // 红色
            {"green", "G", cv::Scalar(0, 255, 0)},    // 绿色
            {"blue",  "B", cv::Scalar(255, 0, 0)}     // 蓝色
        };
        
        // 检测三色物料中心点（面积范围：2000-10000）
        auto filtered = detectThreeColors(resultImg, colorSpecs, 2000, 10000);
        
        // 如果检测成功，发送坐标
        if (!filtered.empty()) {
            serialComm.sendCoordinates(CMD_COLOR, filtered);
        }
    } catch (const std::exception& e) {
        std::cerr << "unit=1处理异常: " << e.what() << std::endl;
    }
}

/**
 * @brief 处理圆环检测模式（MODE_RING）
 * 
 * @details 检测三个同心圆的圆心坐标，用于判断机器人的姿态。
 *          检测结果为左、中、右三个圆心坐标。
 * 
 * @param resultImg 结果图像（会被修改，绘制检测结果）
 * 
 * @note 检测流程：
 *       1. 使用 ThreeRingDetector 检测三组圆心坐标
 *       2. 对三个圆心分别进行卡尔曼滤波
 *       3. 绘制圆圈和标签（L/M/R）
 *       4. 发送坐标到串口
 *       
 * @note 标签含义：
 *       - L: 左侧圆心
 *       - M: 中间圆心
 *       - R: 右侧圆心
 *       
 * @see ThreeRingDetector::detect(), filterPosition(), SerialComm::sendCoordinates()
 */
void VisionSystem::processRing(cv::Mat& resultImg) {
    try {
        // 步骤 1: 检测三组圆心坐标
        auto circlePos = m_threeRingDetector.detect(resultImg);
        if (!circlePos) return;  // 检测失败

        // 步骤 2: 拆分三组圆心坐标，卡尔曼滤波
        std::vector<std::pair<int, int>> filtered;
        for (int idx = 0; idx < 3; ++idx) {
            // 提取圆心坐标（每两个元素为一组）
            auto [fx, fy] = filterPosition(
                static_cast<float>((*circlePos)[idx * 2]),     // X坐标
                static_cast<float>((*circlePos)[idx * 2 + 1]), // Y坐标
                idx);
            filtered.push_back({fx, fy});
        }

        // 步骤 3: 绘制 L/M/R 标签（黄色）
        const char* labels[] = {"L", "M", "R"};
        for (int i = 0; i < 3; ++i) {
            // 绘制圆圈
            cv::circle(resultImg, cv::Point(filtered[i].first, filtered[i].second),
                       8, cv::Scalar(0, 255, 255), -1);
            // 绘制标签
            cv::putText(resultImg, labels[i],
                        cv::Point(filtered[i].first - 5, filtered[i].second - 15),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 255), 1);
        }
        
        // 步骤 4: 发送坐标到串口
        serialComm.sendCoordinates(CMD_RING, filtered);
    } catch (const std::exception& e) {
        std::cerr << "unit=3处理异常: " << e.what() << std::endl;
    }
}

/**
 * @brief 处理停靠点检测模式（MODE_DOCK）
 * 
 * @details 检测停靠点的三个颜色标记（蓝、绿、红），用于引导机器人停靠。
 *          
 * @param resultImg 结果图像（会被修改，绘制检测结果）
 * 
 * @note 检测配置：
 *       - 蓝色：标签 "B"，绘制颜色 (255, 0, 0)
 *       - 绿色：标签 "G"，绘制颜色 (0, 255, 0)
 *       - 红色：标签 "R"，绘制颜色 (0, 0, 255)
 *       - 面积范围：3000-10000 像素
 *       
 * @note 与 processColor 的区别：
 *       - 颜色顺序不同（蓝-绿-红 vs 红-绿-蓝）
 *       - 面积范围不同（3000-10000 vs 2000-10000）
 *       
 * @see detectThreeColors(), SerialComm::sendCoordinates()
 */
void VisionSystem::processDock(cv::Mat& resultImg) {
    try {
        // 停靠点三色识别配置（蓝-绿-红）
        std::vector<std::tuple<std::string, std::string, cv::Scalar>> colorSpecs = {
            {"blue",  "B", cv::Scalar(255, 0, 0)},    // 蓝色
            {"green", "G", cv::Scalar(0, 255, 0)},    // 绿色
            {"red",   "R", cv::Scalar(0, 0, 255)}     // 红色
        };
        
        // 检测三色标记（面积范围：3000-10000）
        auto filtered = detectThreeColors(resultImg, colorSpecs, 3000, 10000);
        
        // 如果检测成功，发送坐标
        if (!filtered.empty()) {
            serialComm.sendCoordinates(CMD_DOCK, filtered);
        }
    } catch (const std::exception& e) {
        std::cerr << "unit=4处理异常: " << e.what() << std::endl;
    }
}

/**
 * @brief 处理二维码识别模式（MODE_QR）
 * 
 * @details 从扫码摄像头读取图像并识别二维码。
 *          如果扫码摄像头不可用，则使用主摄像头图像。
 * 
 * @param resultImg 结果图像（会被修改，绘制识别结果）
 * 
 * @note 识别流程：
 *       1. 尝试从扫码摄像头读取图像
 *       2. 如果扫码摄像头不可用，使用主摄像头图像
 *       3. 使用 QrDetector 识别二维码
 *       4. 如果识别成功，在图像上绘制二维码内容
 *       5. 发送二维码数据到串口
 *       
 * @see CameraManager::readQr(), QrDetector::detect(), SerialComm::sendQrData()
 */
void VisionSystem::processQr(cv::Mat& resultImg) {
    try {
        // 步骤 1: 从扫码摄像头读取图像
        auto [success, qrImg] = camera.readQr();
        
        // 步骤 2: 选择目标图像（扫码摄像头 或 主摄像头）
        cv::Mat targetImg = success ? qrImg : resultImg;
        if (targetImg.empty()) return;  // 图像为空，退出

        // 步骤 3: 识别二维码
        auto qrData = qrDetector.detect(targetImg);
        
        if (qrData) {
            // 识别成功，输出到控制台
            std::cout << "二维码识别成功: " << *qrData << std::endl;
            
            // 在图像上绘制二维码内容（绿色）
            cv::putText(resultImg, "QR: " + *qrData, cv::Point(10, 30),
                        cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
            
            // 发送二维码数据到串口
            serialComm.sendQrData(*qrData);
        }
    } catch (const std::exception& e) {
        std::cerr << "unit=9处理异常: " << e.what() << std::endl;
    }
}

/**
 * @brief 处理单帧图像的核心入口
 * 
 * @details 根据工作模式调用相应的处理函数，并在图像上绘制模式文字。
 *          
 * @param img 输入图像（BGR格式）
 * @param unit 工作模式（默认-1，表示从串口读取）
 *        - MODE_COLOR (1): 三色物料检测
 *        - MODE_RING (3): 圆环检测
 *        - MODE_DOCK (4): 停靠点检测
 *        - MODE_QR (9): 二维码识别
 *        - MODE_IDLE (0): 空闲状态
 *        - < 0: 从串口读取当前工作模式
 * 
 * @return cv::Mat 处理后的图像（包含检测结果和模式文字）
 * 
 * @note 处理流程：
 *       1. 检查图像是否为空
 *       2. 如果未指定模式，从串口读取当前模式
 *       3. 根据模式调用相应的处理函数
 *       4. 在图像底部绘制模式文字
 *       5. 返回处理后的图像
 *       
 * @see processColor(), processRing(), processDock(), processQr()
 */
cv::Mat VisionSystem::processFrame(const cv::Mat& img, int unit) {
    // 步骤 1: 检查图像是否为空
    if (img.empty()) return cv::Mat();

    // 步骤 2: 如果未指定模式，从串口读取当前工作模式
    if (unit < 0) {
        unit = serialComm.unit.load();  // 从原子变量读取
    }

    // 克隆图像（避免修改原图）
    cv::Mat resultImg = img.clone();

    // 步骤 3: 模式分发
    switch (unit) {
        case MODE_COLOR: processColor(resultImg); break;  // 三色物料检测
        case MODE_RING:  processRing(resultImg);  break;  // 圆环检测
        case MODE_DOCK:  processDock(resultImg);  break;  // 停靠点检测
        case MODE_QR:    processQr(resultImg);    break;  // 二维码识别
        default: break;  // 其他模式（包括IDLE）不处理
    }

    // 步骤 4: 绘制模式文字（图像底部）
    const char* modeText = "UNK";  // 未知模式
    switch (unit) {
        case MODE_IDLE:  modeText = "IDLE";  break;  // 空闲
        case MODE_COLOR: modeText = "COLOR"; break;  // 三色物料
        case MODE_RING:  modeText = "RING";  break;  // 圆环
        case MODE_DOCK:  modeText = "DOCK";  break;  // 停靠点
        case MODE_QR:    modeText = "QR";    break;  // 二维码
        default: break;
    }
    
    // 在图像底部绘制模式文字（黄色）
    cv::putText(resultImg, std::string("Mode: ") + modeText,
                cv::Point(10, resultImg.rows - 10),
                cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 255), 2);

    return resultImg;
}
