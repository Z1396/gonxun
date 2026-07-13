// 摄像头管理模块实现
// 使用 cv::VideoCapture，跨平台后端选择（Linux 用 V4L2），自动重连
#include "camera_manager.hpp"

#include <chrono>
#include <iostream>
#include <thread>

// 降级分辨率列表
const std::vector<std::pair<int, int>> FALLBACK_RESOLUTIONS = {
    {640, 480},
    {320, 240},
    {160, 120},
};

int CameraManager::getPreferredBackend() {
#ifdef _WIN32
    return cv::CAP_DSHOW;   // Windows: DirectShow 更稳定
#elif defined(__linux__)
    return cv::CAP_V4L2;   // Linux: V4L2
#else
    return cv::CAP_ANY;    // macOS/其他: 自动选择
#endif
}

CameraManager::CameraManager(int main_index, int qr_index,
                             int main_width, int main_height,
                             int qr_width, int qr_height,
                             int max_reconnect, double reconnect_delay)
    : main_index_(main_index),
      qr_index_(qr_index),
      main_resolution_(main_width, main_height),
      qr_resolution_(qr_width, qr_height),
      max_reconnect_(max_reconnect),
      reconnect_delay_(reconnect_delay),
      backend_(getPreferredBackend()),
      main_fail_count_(0),
      qr_fail_count_(0),
      fail_threshold_(30) {}

CameraManager::~CameraManager() {
    close();
}

void CameraManager::configureCapture(cv::VideoCapture& cap,
                                     const std::pair<int, int>& resolution) const {
    if (!cap.isOpened()) {
        return;
    }
    cap.set(cv::CAP_PROP_FRAME_WIDTH, static_cast<double>(resolution.first));
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, static_cast<double>(resolution.second));
    cap.set(cv::CAP_PROP_BUFFERSIZE, 1.0);
#ifdef __linux__
    // Jetson/Linux 曝光设置（减少50Hz灯光频闪）
    cap.set(cv::CAP_PROP_AUTO_EXPOSURE, 1.0);    // 手动曝光模式
    cap.set(cv::CAP_PROP_EXPOSURE, 120.0);        // Jetson 用正值
    cap.set(cv::CAP_PROP_GAIN, 15.0);             // 增益
#else
    // Windows
    cap.set(cv::CAP_PROP_AUTO_EXPOSURE, 0.25);   // 手动曝光
    cap.set(cv::CAP_PROP_EXPOSURE, -5.0);        // Windows 用负值
#endif
}

std::unique_ptr<cv::VideoCapture> CameraManager::openOne(
    int index, const std::pair<int, int>& resolution, const std::string& name) const {
    // 尝试不同后端：首选后端 + 自动后端
    const int backends[2] = {backend_, cv::CAP_ANY};
    std::unique_ptr<cv::VideoCapture> cap;
    for (int be : backends) {
        cap = std::make_unique<cv::VideoCapture>(index, be);
        if (cap->isOpened()) {
            break;
        }
        cap.reset();
    }
    if (!cap) {
        std::cout << "[摄像头] 警告：无法打开" << name << "摄像头 " << index << std::endl;
        return nullptr;
    }
    configureCapture(*cap, resolution);

    // 验证实际分辨率
    int actual_w = static_cast<int>(cap->get(cv::CAP_PROP_FRAME_WIDTH));
    int actual_h = static_cast<int>(cap->get(cv::CAP_PROP_FRAME_HEIGHT));
    if (actual_w != resolution.first || actual_h != resolution.second) {
        std::cout << "[摄像头] " << name << "摄像头分辨率降级: 请求"
                  << resolution.first << "x" << resolution.second
                  << ", 实际" << actual_w << "x" << actual_h << std::endl;
    }
    std::cout << "[摄像头] " << name << "摄像头已打开: index=" << index
              << ", 分辨率=" << actual_w << "x" << actual_h << std::endl;
    return cap;
}

void CameraManager::open() {
    cap_ = openOne(main_index_, main_resolution_, "主");
    cap2_ = openOne(qr_index_, qr_resolution_, "扫码");
}

void CameraManager::close() {
    {
        std::lock_guard<std::mutex> lock(main_lock_);
        if (cap_) {
            cap_->release();
        }
        cap_.reset();
    }
    {
        std::lock_guard<std::mutex> lock(qr_lock_);
        if (cap2_) {
            cap2_->release();
        }
        cap2_.reset();
    }
    main_fail_count_ = 0;
    qr_fail_count_ = 0;
}

void CameraManager::reconnect(int index,
                              const std::pair<int, int>& resolution,
                              const std::string& name,
                              std::unique_ptr<cv::VideoCapture>& current_cap,
                              std::mutex& lock) {
    std::lock_guard<std::mutex> lk(lock);
    // 已由其他线程重连，直接返回
    if (current_cap) {
        return;
    }
    std::cout << "[摄像头] 正在重连" << name << "摄像头 " << index << "..." << std::endl;
    // 持锁等待，避免重连期间其他线程同时读取（与 Python 行为一致）
    std::this_thread::sleep_for(
        std::chrono::duration<double>(reconnect_delay_));
    current_cap = openOne(index, resolution, name);
    if (current_cap) {
        std::cout << "[摄像头] " << name << "摄像头重连成功" << std::endl;
    } else {
        std::cout << "[摄像头] " << name << "摄像头重连失败" << std::endl;
    }
}

std::pair<bool, cv::Mat> CameraManager::readMain() {
    {
        std::lock_guard<std::mutex> lock(main_lock_);
        if (cap_ && cap_->isOpened()) {
            cv::Mat frame;
            bool ret = cap_->read(frame);
            if (ret) {
                main_fail_count_ = 0;
                return {true, std::move(frame)};
            }
            main_fail_count_++;
            if (main_fail_count_ >= fail_threshold_) {
                main_fail_count_ = 0;
                cap_->release();
                cap_.reset();
            }
        }
    }
    // 锁外检查并尝试重连
    if (!cap_) {
        reconnect(main_index_, main_resolution_, "主", cap_, main_lock_);
    }
    return {false, cv::Mat()};
}

std::pair<bool, cv::Mat> CameraManager::readQr() {
    {
        std::lock_guard<std::mutex> lock(qr_lock_);
        if (cap2_ && cap2_->isOpened()) {
            cv::Mat frame;
            bool ret = cap2_->read(frame);
            if (ret) {
                qr_fail_count_ = 0;
                return {true, std::move(frame)};
            }
            qr_fail_count_++;
            if (qr_fail_count_ >= fail_threshold_) {
                qr_fail_count_ = 0;
                cap2_->release();
                cap2_.reset();
            }
        }
    }
    // 锁外检查并尝试重连
    if (!cap2_) {
        reconnect(qr_index_, qr_resolution_, "扫码", cap2_, qr_lock_);
    }
    return {false, cv::Mat()};
}
