/**
 * @file camera_manager.cpp
 * @brief 摄像头管理模块实现文件
 * 
 * @details 本文件实现了智能物流搬运系统的摄像头管理功能。
 *          核心功能：
 *          - 双摄像头管理：主摄像头（目标检测）、扫码摄像头（二维码识别）
 *          - 跨平台支持：Windows (DirectShow)、Linux (V4L2)、macOS (自动选择)
 *          - 自动重连机制：检测失败后自动尝试重新连接
 *          - 分辨率配置：支持分辨率降级、曝光参数调节
 *          - 线程安全设计：使用互斥锁保护摄像头资源
 * 
 * @author 智能物流搬运系统开发团队
 * @version 1.0
 * @date 2025-01-01
 * 
 * @note 修改历史：
 *       - 2025-01-01: 初始版本，实现基本摄像头管理功能
 *       - 2025-03-15: 增加自动重连机制和线程安全保护
 * 
 * @see camera_manager.hpp
 */
#include "camera_manager.hpp"

#include <chrono>
#include <iostream>
#include <thread>

/**
 * @brief 降级分辨率列表
 * 
 * @details 当摄像头无法以目标分辨率打开时，依次尝试列表中的较低分辨率。
 *          分辨率从高到低排列，确保在性能受限时仍能正常工作。
 * 
 * @note 列表顺序：
 *       - 640x480: 标准分辨率，适合大多数应用场景
 *       - 320x240: 中等分辨率，降低计算负载
 *       - 160x120: 最低分辨率，用于极端性能受限场景
 */
const std::vector<std::pair<int, int>> FALLBACK_RESOLUTIONS = {
    {640, 480},  // 标准分辨率
    {320, 240},  // 中等分辨率
    {160, 120},  // 最低分辨率
};

/**
 * @brief 获取当前平台的推荐摄像头后端
 * 
 * @details 根据操作系统自动选择最优的摄像头后端：
 *          - Windows: 使用 DirectShow (cv::CAP_DSHOW)，稳定性和兼容性最佳
 *          - Linux: 使用 V4L2 (cv::CAP_V4L2)，包括 Jetson Nano 等嵌入式平台
 *          - macOS/其他: 使用自动选择 (cv::CAP_ANY)
 * 
 * @return int OpenCV 摄像头后端标识符
 * 
 * @note 平台检测通过预处理器宏实现，编译时确定后端类型
 * @see cv::VideoCapture, cv::CAP_DSHOW, cv::CAP_V4L2
 */
int CameraManager::getPreferredBackend() {
#ifdef _WIN32
    return cv::CAP_DSHOW;   // Windows: DirectShow 更稳定
#elif defined(__linux__)
    return cv::CAP_V4L2;   // Linux: V4L2
#else
    return cv::CAP_ANY;    // macOS/其他: 自动选择
#endif
}

/**
 * @brief 构造函数，初始化摄像头管理器
 * 
 * @details 配置双摄像头参数，包括索引、分辨率、重连策略等。
 *          采用延迟初始化策略，构造时不立即打开摄像头。
 * 
 * @param main_index 主摄像头索引（用于目标检测），通常为 0 或 1
 * @param qr_index 扫码摄像头索引（用于二维码识别），通常为 1 或 2
 * @param main_width 主摄像头分辨率宽度（像素）
 * @param main_height 主摄像头分辨率高度（像素）
 * @param qr_width 扫码摄像头分辨率宽度（像素）
 * @param qr_height 扫码摄像头分辨率高度（像素）
 * @param max_reconnect 最大重连次数（保留参数，当前未使用）
 * @param reconnect_delay 重连延迟时间（秒），避免频繁重连
 * 
 * @note 默认失败阈值为 30 次，超过后触发重连
 * @see open(), close()
 */
CameraManager::CameraManager(int main_index, int qr_index,
                             int main_width, int main_height,
                             int qr_width, int qr_height,
                             int max_reconnect, double reconnect_delay)
    : main_index_(main_index),             // 主摄像头索引
      qr_index_(qr_index),                 // 扫码摄像头索引
      main_resolution_(main_width, main_height),  // 主摄像头分辨率
      qr_resolution_(qr_width, qr_height), // 扫码摄像头分辨率
      max_reconnect_(max_reconnect),       // 最大重连次数
      reconnect_delay_(reconnect_delay),   // 重连延迟时间（秒）
      backend_(getPreferredBackend()),     // 平台推荐后端
      main_fail_count_(0),                 // 主摄像头失败计数器
      qr_fail_count_(0),                   // 扫码摄像头失败计数器
      fail_threshold_(30) {}               // 失败阈值（30次）

CameraManager::~CameraManager() {
    close();
}

/**
 * @brief 配置摄像头参数
 * 
 * @details 设置分辨率、缓冲区大小、曝光参数等。
 *          针对 Linux (Jetson) 和 Windows 平台采用不同的曝光配置策略。
 * 
 * @param cap 摄像头捕获对象的引用
 * @param resolution 目标分辨率 (宽度, 高度)
 * 
 * @note 曝光配置说明：
 *       - Linux/Jetson: 手动曝光模式，曝光值 120（正值），增益 15
 *       - Windows: 手动曝光模式，曝光值 -5（负值）
 *       - 目的：减少 50Hz 灯光频闪对图像质量的影响
 *       
 * @note 缓冲区大小设为 1，确保获取最新帧而非旧帧
 */
void CameraManager::configureCapture(cv::VideoCapture& cap,
                                     const std::pair<int, int>& resolution) const {
    // 检查摄像头是否已打开
    if (!cap.isOpened()) {
        return;
    }
    
    // 设置分辨率
    cap.set(cv::CAP_PROP_FRAME_WIDTH, static_cast<double>(resolution.first));
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, static_cast<double>(resolution.second));
    
    // 设置缓冲区大小为 1，确保读取最新帧
    cap.set(cv::CAP_PROP_BUFFERSIZE, 1.0);
    
#ifdef __linux__
    // Jetson/Linux 曝光设置（减少50Hz灯光频闪）
    cap.set(cv::CAP_PROP_AUTO_EXPOSURE, 1.0);    // 手动曝光模式
    cap.set(cv::CAP_PROP_EXPOSURE, 120.0);        // Jetson 用正值（单位：曝光时间单位）
    cap.set(cv::CAP_PROP_GAIN, 15.0);             // 增益（提高低光环境下的亮度）
#else
    // Windows 曝光设置
    cap.set(cv::CAP_PROP_AUTO_EXPOSURE, 0.25);   // 手动曝光模式
    cap.set(cv::CAP_PROP_EXPOSURE, -5.0);        // Windows 用负值（单位：对数曝光值）
#endif
}

/**
 * @brief 打开单个摄像头
 * 
 * @details 尝试使用平台推荐后端和自动后端打开摄像头，
 *          如果成功则配置参数并验证实际分辨率。
 * 
 * @param index 摄像头索引（0, 1, 2...）
 * @param resolution 目标分辨率 (宽度, 高度)
 * @param name 摄像头名称（用于日志输出，如 "主"、"扫码"）
 * 
 * @return std::unique_ptr<cv::VideoCapture> 成功返回摄像头对象，失败返回 nullptr
 * 
 * @note 后端尝试顺序：
 *       1. 平台推荐后端（DirectShow/V4L2）
 *       2. 自动选择后端（CAP_ANY）
 *       
 * @note 分辨率降级：
 *       如果摄像头不支持目标分辨率，将使用实际支持的分辨率，
 *       并输出降级警告日志。
 */
std::unique_ptr<cv::VideoCapture> CameraManager::openOne(
    int index, const std::pair<int, int>& resolution, const std::string& name) const {
    
    // 尝试不同后端：首选后端 + 自动后端
    const int backends[2] = {backend_, cv::CAP_ANY};
    std::unique_ptr<cv::VideoCapture> cap;
    
    // 按顺序尝试后端
    for (int be : backends) {
        cap = std::make_unique<cv::VideoCapture>(index, be);
        if (cap->isOpened()) {
            break;  // 成功打开，退出循环
        }
        cap.reset();  // 失败，重置指针
    }
    
    // 检查是否成功打开
    if (!cap) {
        std::cout << "[摄像头] 警告：无法打开" << name << "摄像头 " << index << std::endl;
        return nullptr;
    }
    
    // 配置摄像头参数
    configureCapture(*cap, resolution);

    // 验证实际分辨率
    int actual_w = static_cast<int>(cap->get(cv::CAP_PROP_FRAME_WIDTH));
    int actual_h = static_cast<int>(cap->get(cv::CAP_PROP_FRAME_HEIGHT));
    
    // 检查是否发生分辨率降级
    if (actual_w != resolution.first || actual_h != resolution.second) {
        std::cout << "[摄像头] " << name << "摄像头分辨率降级: 请求"
                  << resolution.first << "x" << resolution.second
                  << ", 实际" << actual_w << "x" << actual_h << std::endl;
    }
    
    // 输出成功日志
    std::cout << "[摄像头] " << name << "摄像头已打开: index=" << index
              << ", 分辨率=" << actual_w << "x" << actual_h << std::endl;
    
    return cap;
}

/**
 * @brief 打开所有摄像头
 * 
 * @details 调用 openOne() 分别打开主摄像头和扫码摄像头。
 *          如果某个摄像头打开失败，不会影响另一个摄像头的使用。
 * 
 * @note 调用时机：
 *       - 程序启动时调用
 *       - 手动重新连接时调用
 *       
 * @see openOne(), close()
 */
void CameraManager::open() {
    // 打开主摄像头（用于目标检测）
    cap_ = openOne(main_index_, main_resolution_, "主");
    // 打开扫码摄像头（用于二维码识别）
    cap2_ = openOne(qr_index_, qr_resolution_, "扫码");
}

/**
 * @brief 关闭所有摄像头
 * 
 * @details 线程安全地释放摄像头资源，重置失败计数器。
 *          使用互斥锁保护资源访问，避免多线程竞争。
 * 
 * @note 调用时机：
 *       - 程序退出时
 *       - 切换摄像头配置时
 *       - 发生不可恢复的错误时
 *       
 * @note 线程安全：
 *       使用两个独立的锁分别保护主摄像头和扫码摄像头，
 *       避免一个锁阻塞另一个摄像头的操作。
 */
void CameraManager::close() {
    // 关闭主摄像头
    {
        std::lock_guard<std::mutex> lock(main_lock_);
        if (cap_) {
            cap_->release();  // 释放摄像头资源
        }
        cap_.reset();  // 重置智能指针
    }
    
    // 关闭扫码摄像头
    {
        std::lock_guard<std::mutex> lock(qr_lock_);
        if (cap2_) {
            cap2_->release();  // 释放摄像头资源
        }
        cap2_.reset();  // 重置智能指针
    }
    
    // 重置失败计数器
    main_fail_count_ = 0;
    qr_fail_count_ = 0;
}

/**
 * @brief 重连摄像头
 * 
 * @details 当摄像头读取失败时，自动尝试重新连接。
 *          使用互斥锁保护重连过程，避免多线程竞争。
 *          持锁等待 reconnect_delay_ 秒后尝试重连。
 * 
 * @param index 摄像头索引
 * @param resolution 目标分辨率
 * @param name 摄像头名称（用于日志输出）
 * @param current_cap 当前摄像头对象的引用（智能指针）
 * @param lock 保护该摄像头的互斥锁
 * 
 * @note 线程安全设计：
 *       1. 使用互斥锁保护重连过程
 *       2. 双重检查：进入函数后再次检查 current_cap，避免重复重连
 *       3. 持锁等待：等待期间锁定互斥锁，避免其他线程同时读取
 *       
 * @note 重连延迟：
 *       reconnect_delay_ 秒的等待时间用于给硬件恢复时间，
 *       避免立即重连导致的问题。
 */
void CameraManager::reconnect(int index,
                              const std::pair<int, int>& resolution,
                              const std::string& name,
                              std::unique_ptr<cv::VideoCapture>& current_cap,
                              std::mutex& lock) {
    // 加锁保护重连过程
    std::lock_guard<std::mutex> lk(lock);
    
    // 双重检查：已由其他线程重连，直接返回
    if (current_cap) {
        return;
    }
    
    std::cout << "[摄像头] 正在重连" << name << "摄像头 " << index << "..." << std::endl;
    
    // 持锁等待，避免重连期间其他线程同时读取（与 Python 行为一致）
    std::this_thread::sleep_for(
        std::chrono::duration<double>(reconnect_delay_));
    
    // 尝试重新打开摄像头
    current_cap = openOne(index, resolution, name);
    
    // 输出重连结果
    if (current_cap) {
        std::cout << "[摄像头] " << name << "摄像头重连成功" << std::endl;
    } else {
        std::cout << "[摄像头] " << name << "摄像头重连失败" << std::endl;
    }
}

/**
 * @brief 读取主摄像头帧
 * 
 * @details 从主摄像头读取一帧图像，失败时自动计数并触发重连机制。
 *          使用互斥锁保护摄像头资源，确保线程安全。
 * 
 * @return std::pair<bool, cv::Mat> 
 *         - first: 是否成功读取（true/false）
 *         - second: 图像帧（成功时有效，失败时为空）
 * 
 * @note 失败检测机制：
 *       1. 每次读取失败，失败计数器 +1
 *       2. 失败计数达到 fail_threshold_（默认 30 次）时，释放摄像头
 *       3. 释放后，下次调用会触发重连
 *       
 * @note 线程安全：
 *       - 使用互斥锁保护摄像头读取操作
 *       - 锁外检查摄像头状态，避免持锁时间过长
 *       - 重连函数内部会再次加锁
 *       
 * @see reconnect(), readQr()
 */
std::pair<bool, cv::Mat> CameraManager::readMain() {
    // 加锁保护读取操作
    {
        std::lock_guard<std::mutex> lock(main_lock_);
        
        // 检查摄像头是否可用
        if (cap_ && cap_->isOpened()) {
            cv::Mat frame;
            bool ret = cap_->read(frame);  // 读取一帧
            
            if (ret) {
                // 读取成功，重置失败计数器
                main_fail_count_ = 0;
                return {true, std::move(frame)};
            }
            
            // 读取失败，增加失败计数
            main_fail_count_++;
            
            // 失败次数超过阈值，释放摄像头资源
            if (main_fail_count_ >= fail_threshold_) {
                main_fail_count_ = 0;  // 重置计数器
                cap_->release();       // 释放摄像头
                cap_.reset();          // 重置智能指针
            }
        }
    }
    
    // 锁外检查并尝试重连（避免持锁时间过长）
    if (!cap_) {
        reconnect(main_index_, main_resolution_, "主", cap_, main_lock_);
    }
    
    return {false, cv::Mat()};  // 返回失败状态
}

/**
 * @brief 读取扫码摄像头帧
 * 
 * @details 从扫码摄像头读取一帧图像，用于二维码识别。
 *          失败时自动计数并触发重连机制。
 *          使用独立的互斥锁保护扫码摄像头资源。
 * 
 * @return std::pair<bool, cv::Mat> 
 *         - first: 是否成功读取（true/false）
 *         - second: 图像帧（成功时有效，失败时为空）
 * 
 * @note 与 readMain() 的区别：
 *       - 使用独立的摄像头对象 (cap2_)
 *       - 使用独立的互斥锁 (qr_lock_)
 *       - 使用独立的失败计数器 (qr_fail_count_)
 *       
 * @note 失败检测机制：
 *       1. 每次读取失败，失败计数器 +1
 *       2. 失败计数达到 fail_threshold_（默认 30 次）时，释放摄像头
 *       3. 释放后，下次调用会触发重连
 *       
 * @see reconnect(), readMain()
 */
std::pair<bool, cv::Mat> CameraManager::readQr() {
    // 加锁保护读取操作
    {
        std::lock_guard<std::mutex> lock(qr_lock_);
        
        // 检查摄像头是否可用
        if (cap2_ && cap2_->isOpened()) {
            cv::Mat frame;
            bool ret = cap2_->read(frame);  // 读取一帧
            
            if (ret) {
                // 读取成功，重置失败计数器
                qr_fail_count_ = 0;
                return {true, std::move(frame)};
            }
            
            // 读取失败，增加失败计数
            qr_fail_count_++;
            
            // 失败次数超过阈值，释放摄像头资源
            if (qr_fail_count_ >= fail_threshold_) {
                qr_fail_count_ = 0;  // 重置计数器
                cap2_->release();    // 释放摄像头
                cap2_.reset();       // 重置智能指针
            }
        }
    }
    
    // 锁外检查并尝试重连（避免持锁时间过长）
    if (!cap2_) {
        reconnect(qr_index_, qr_resolution_, "扫码", cap2_, qr_lock_);
    }
    
    return {false, cv::Mat()};  // 返回失败状态
}
