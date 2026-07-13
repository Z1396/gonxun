/**
 * 视觉系统核心调度模块实现
 * 对应 Python: vision/system.py
 */
#include "vision_system.hpp"
#include "config.hpp"
#include <iostream>
#include <climits>

VisionSystem::VisionSystem(bool serialMock, const std::string& serialPort,
                           int baudrate, int mainCamera, int qrCamera)
    // 初始化串口
    : serialComm(serialMock,
                 serialPort.empty() ? config::SERIAL_PORT : serialPort,
                 baudrate,
                 MODE_IDLE,
                 config::SERIAL_MOCK_CYCLE),
      // 初始化摄像头
      camera(mainCamera >= 0 ? mainCamera : config::CAMERA_MAIN_INDEX,
             qrCamera >= 0 ? qrCamera : config::CAMERA_QR_INDEX,
             config::CAMERA_MAIN_WIDTH, config::CAMERA_MAIN_HEIGHT,
             config::CAMERA_QR_WIDTH, config::CAMERA_QR_HEIGHT),
      // 初始化 YOLO 检测器
      m_yoloDetector(config::YOLO_TS_MODEL_PATH,
                     config::YOLO_IMGSZ,
                     config::YOLO_CONF_THRESHOLD),
      // 初始化卡尔曼滤波器
      m_kalmanFilters{
          KalmanFilter(config::KALMAN_Q, config::KALMAN_R),
          KalmanFilter(config::KALMAN_Q, config::KALMAN_R),
          KalmanFilter(config::KALMAN_Q, config::KALMAN_R)
      } {

    std::cout << "VisionSystem 初始化完成" << std::endl;
    if (!m_yoloDetector.isAvailable()) {
        std::cerr << "[警告] YOLO 模型未加载，颜色检测功能不可用" << std::endl;
    }
}

std::pair<int, int> VisionSystem::filterPosition(float x, float y, int kfIndex) {
    cv::Matx21f z(x, y);
    auto filtered = m_kalmanFilters[kfIndex].filter(z);
    return {static_cast<int>(filtered(0)), static_cast<int>(filtered(1))};
}

std::vector<std::pair<int, int>> VisionSystem::detectThreeColors(
    cv::Mat& img,
    const std::vector<std::tuple<std::string, std::string, cv::Scalar>>& colorSpecs,
    int minArea, int maxArea) {

    std::vector<std::pair<int, int>> positions;
    for (const auto& [color, label, drawColor] : colorSpecs) {
        auto pos = m_yoloDetector.detectCenter(img, color, minArea, maxArea);
        if (!pos) return {};  // 任意颜色缺失，返回空
        positions.push_back({pos->x, pos->y});
    }

    // 卡尔曼滤波 + 绘制
    std::vector<std::pair<int, int>> filtered;
    for (size_t idx = 0; idx < positions.size(); ++idx) {
        auto [fx, fy] = filterPosition(
            static_cast<float>(positions[idx].first),
            static_cast<float>(positions[idx].second),
            static_cast<int>(idx));
        filtered.push_back({fx, fy});

        const auto& [color, label, drawColor] = colorSpecs[idx];
        cv::circle(img, cv::Point(fx, fy), 8, drawColor, -1);
        cv::putText(img, label, cv::Point(fx - 5, fy - 15),
                    cv::FONT_HERSHEY_SIMPLEX, 0.5, drawColor, 1);
    }
    return filtered;
}

void VisionSystem::processColor(cv::Mat& resultImg) {
    try {
        // 三色识别配置: 颜色名, 标签, 绘制颜色(BGR)
        std::vector<std::tuple<std::string, std::string, cv::Scalar>> colorSpecs = {
            {"red",   "R", cv::Scalar(0, 0, 255)},
            {"green", "G", cv::Scalar(0, 255, 0)},
            {"blue",  "B", cv::Scalar(255, 0, 0)}
        };
        auto filtered = detectThreeColors(resultImg, colorSpecs, 2000, 10000);
        if (!filtered.empty()) {
            serialComm.sendCoordinates(CMD_COLOR, filtered);
        }
    } catch (const std::exception& e) {
        std::cerr << "unit=1处理异常: " << e.what() << std::endl;
    }
}

void VisionSystem::processRing(cv::Mat& resultImg) {
    try {
        auto circlePos = m_threeRingDetector.detect(resultImg);
        if (!circlePos) return;

        // 拆分三组圆心坐标，卡尔曼滤波
        std::vector<std::pair<int, int>> filtered;
        for (int idx = 0; idx < 3; ++idx) {
            auto [fx, fy] = filterPosition(
                static_cast<float>((*circlePos)[idx * 2]),
                static_cast<float>((*circlePos)[idx * 2 + 1]),
                idx);
            filtered.push_back({fx, fy});
        }

        // 绘制 L/M/R 标签
        const char* labels[] = {"L", "M", "R"};
        for (int i = 0; i < 3; ++i) {
            cv::circle(resultImg, cv::Point(filtered[i].first, filtered[i].second),
                       8, cv::Scalar(0, 255, 255), -1);
            cv::putText(resultImg, labels[i],
                        cv::Point(filtered[i].first - 5, filtered[i].second - 15),
                        cv::FONT_HERSHEY_SIMPLEX, 0.5, cv::Scalar(0, 255, 255), 1);
        }
        serialComm.sendCoordinates(CMD_RING, filtered);
    } catch (const std::exception& e) {
        std::cerr << "unit=3处理异常: " << e.what() << std::endl;
    }
}

void VisionSystem::processDock(cv::Mat& resultImg) {
    try {
        std::vector<std::tuple<std::string, std::string, cv::Scalar>> colorSpecs = {
            {"blue",  "B", cv::Scalar(255, 0, 0)},
            {"green", "G", cv::Scalar(0, 255, 0)},
            {"red",   "R", cv::Scalar(0, 0, 255)}
        };
        auto filtered = detectThreeColors(resultImg, colorSpecs, 3000, 10000);
        if (!filtered.empty()) {
            serialComm.sendCoordinates(CMD_DOCK, filtered);
        }
    } catch (const std::exception& e) {
        std::cerr << "unit=4处理异常: " << e.what() << std::endl;
    }
}

void VisionSystem::processQr(cv::Mat& resultImg) {
    try {
        auto [success, qrImg] = camera.readQr();
        cv::Mat targetImg = success ? qrImg : resultImg;
        if (targetImg.empty()) return;

        auto qrData = qrDetector.detect(targetImg);
        if (qrData) {
            std::cout << "二维码识别成功: " << *qrData << std::endl;
            cv::putText(resultImg, "QR: " + *qrData, cv::Point(10, 30),
                        cv::FONT_HERSHEY_SIMPLEX, 1, cv::Scalar(0, 255, 0), 2);
            serialComm.sendQrData(*qrData);
        }
    } catch (const std::exception& e) {
        std::cerr << "unit=9处理异常: " << e.what() << std::endl;
    }
}

cv::Mat VisionSystem::processFrame(const cv::Mat& img, int unit) {
    if (img.empty()) return cv::Mat();

    // 未指定模式则从串口读取
    if (unit < 0) {
        unit = serialComm.unit.load();
    }

    cv::Mat resultImg = img.clone();

    // 模式分发
    switch (unit) {
        case MODE_COLOR: processColor(resultImg); break;
        case MODE_RING:  processRing(resultImg);  break;
        case MODE_DOCK:  processDock(resultImg);  break;
        case MODE_QR:    processQr(resultImg);    break;
        default: break;
    }

    // 绘制模式文字
    const char* modeText = "UNK";
    switch (unit) {
        case MODE_IDLE:  modeText = "IDLE";  break;
        case MODE_COLOR: modeText = "COLOR"; break;
        case MODE_RING:  modeText = "RING";  break;
        case MODE_DOCK:  modeText = "DOCK";  break;
        case MODE_QR:    modeText = "QR";    break;
        default: break;
    }
    cv::putText(resultImg, std::string("Mode: ") + modeText,
                cv::Point(10, resultImg.rows - 10),
                cv::FONT_HERSHEY_SIMPLEX, 0.7, cv::Scalar(0, 255, 255), 2);

    return resultImg;
}
