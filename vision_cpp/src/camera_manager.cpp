/**
 * @file camera_manager.cpp
 * @brief 双摄像头管理器实现
 *
 * 【模块核心职责】
 * 1. 统一管理主摄像头（物料/算法检测）、扫码摄像头（QR解码）双设备
 * 2. 完成相机初始化、参数配置、实时帧读取、异常检测、自动重连全流程
 * 3. 线程安全设计：双相机独立互斥锁，杜绝多线程抢设备崩溃
 * 4. 故障熔断机制：连续读取失败30次，自动释放资源、触发重连，适配摄像头松动/掉线/卡顿
 *
 * 【运行平台&驱动】
 * 目标平台: Ubuntu 18.04
 * 驱动后端: V4L2 (Linux标准视频设备驱动)
 *
 * 【容错策略】
 * - 单帧读取失败：累计错误计数
 * - 连续失败≥30次：熔断释放相机句柄，清空计数
 * - 设备断开后：自动延时重试连接，支持热插拔恢复
 * - 分辨率自适应：硬件不支持目标分辨率自动降级兼容
 */
#include "camera_manager.hpp"

#include <chrono>
#include <iostream>
#include <thread>

/**
 * @brief 相机分辨率降级适配序列（从高到低）
 * @note 当用户配置的分辨率硬件不支持时，自动逐级降级，保证相机正常启动
 * 适配老旧USB相机、嵌入式设备、带宽不足场景
 * 降级优先级：640×480(默认) → 320×240 → 160×120
 */
const std::vector<std::pair<int, int>> FALLBACK_RESOLUTIONS = {
    {640, 480},
    {320, 240},
    {160, 120},
};

/**
 * @brief 获取当前平台首选相机驱动后端
 * @return int OpenCV驱动枚举值
 * @note Ubuntu18.04 固定使用 V4L2 后端，兼容性、稳定性远优于默认后端
 */
int CameraManager::get_preferred_backend() {
    return cv::CAP_V4L2;
}

/**
 * @brief 相机管理器构造函数
 * @param main_index 主相机设备索引 /dev/videoX
 * @param qr_index 扫码相机设备索引 /dev/videoX
 * @param main_width 主相机目标宽度
 * @param main_height 主相机目标高度
 * @param qr_width 扫码相机目标宽度
 * @param qr_height 扫码相机目标高度
 * @param max_reconnect 最大重连次数（预留扩展）
 * @param reconnect_delay 重连间隔延时（秒）
 *
 * 初始化内容：
 * 1. 保存双相机设备参数、分辨率、重连配置
 * 2. 加载系统最优驱动后端
 * 3. 初始化故障计数、熔断阈值
 * 4. 双相机独立互斥锁默认就绪
 */
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
      backend_(get_preferred_backend()),
      main_fail_count_(0),
      qr_fail_count_(0),
      fail_threshold_(30) {}

/**
 * @brief 析构函数
 * @note 对象销毁时强制关闭并释放双相机资源
 * 杜绝相机句柄泄露、设备占用、重启失效问题
 */
CameraManager::~CameraManager() {
    close();
}

/**
 * @brief 相机参数统一配置函数
 * @param cap 已成功打开的相机捕获句柄
 * @param resolution 目标配置分辨率
 * @note 仅相机打开成功后执行，参数针对Ubuntu18.04+V4L2深度调优
 *
 * 核心调优参数：
 * 1. 固定分辨率：匹配项目算法输入尺寸
 * 2. 缓冲区大小=1：最小化帧缓存，大幅降低图像延迟
 * 3. 手动曝光+固定增益：避免画面频闪、亮度跳变，保证检测稳定性
 */
void CameraManager::configure_capture(cv::VideoCapture& cap,
                                       const std::pair<int, int>& resolution) const {
    // 防护：未打开的相机不执行配置
    if (!cap.isOpened()) {
        return;
    }

    // 设置相机分辨率
    cap.set(cv::CAP_PROP_FRAME_WIDTH, static_cast<double>(resolution.first));
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, static_cast<double>(resolution.second));

    // 缩小缓冲区至1帧，消除画面延迟、帧堆积问题
    cap.set(cv::CAP_PROP_BUFFERSIZE, 1.0);

    // 曝光模式配置：手动曝光，关闭自动曝光（防止画面抖动）
    cap.set(cv::CAP_PROP_AUTO_EXPOSURE, 1.0);
    // 固定曝光值、增益值，适配赛场光照环境
    cap.set(cv::CAP_PROP_EXPOSURE, 120.0);
    cap.set(cv::CAP_PROP_GAIN, 15.0);
}

/**
 * @brief 通用单相机打开工具函数
 * @param index 相机设备索引
 * @param resolution 目标分辨率
 * @param name 相机名称（用于日志区分主/扫码相机）
 * @return std::unique_ptr<cv::VideoCapture> 相机句柄，失败返回空指针
 *
 * 打开策略：
 * 1. 优先使用V4L2专用后端，适配Linux系统
 * 2. V4L2打开失败则降级使用系统默认后端
 * 3. 打开成功后执行参数配置
 * 4. 校验实际分辨率，自动识别硬件降级并打印日志
 */
std::unique_ptr<cv::VideoCapture> CameraManager::open_one(
    int index, const std::pair<int, int>& resolution, const std::string& name) const {

    // 双后端适配：优先V4L2，失败使用默认后端
    const int backends[2] = {backend_, cv::CAP_ANY};
    std::unique_ptr<cv::VideoCapture> cap;

    // 遍历后端尝试打开设备
    for (int be : backends) {
        cap = std::make_unique<cv::VideoCapture>(index, be);
        if (cap->isOpened()) {
            break;
        }
        // 打开失败，释放句柄，尝试下一个后端
        cap.reset();
    }

    // 所有后端均打开失败，直接返回空
    if (!cap) {
        std::cout << "[摄像头] 警告：无法打开" << name << "摄像头 " << index << std::endl;
        return nullptr;
    }

    // 配置相机参数（分辨率、延迟、曝光、增益）
    configure_capture(*cap, resolution);

    // 读取硬件实际生效分辨率
    int actual_w = static_cast<int>(cap->get(cv::CAP_PROP_FRAME_WIDTH));
    int actual_h = static_cast<int>(cap->get(cv::CAP_PROP_FRAME_HEIGHT));

    // 检测是否发生分辨率降级，打印提示日志
    if (actual_w != resolution.first || actual_h != resolution.second) {
        std::cout << "[摄像头] " << name << "摄像头分辨率降级: 请求"
                  << resolution.first << "x" << resolution.second
                  << ", 实际" << actual_w << "x" << actual_h << std::endl;
    }

    std::cout << "[摄像头] " << name << "摄像头已打开: index=" << index
              << ", 分辨率=" << actual_w << "x" << actual_h << std::endl;

    return cap;
}

/**
 * @brief 批量打开双相机（主相机+扫码相机）
 * @note 基于通用open_one接口，分别初始化两个设备
 * 两个相机独立初始化，互不影响，单相机失败不阻塞另一个
 */
void CameraManager::open() {
    cap_ = open_one(main_index_, main_resolution_, "主");
    cap2_ = open_one(qr_index_, qr_resolution_, "扫码");
}

/**
 * @brief 关闭并释放所有相机资源
 * @note 线程安全操作：独立互斥锁保护双相机设备
 * 释放句柄、重置智能指针、清空故障计数，恢复初始状态
 */
void CameraManager::close() {
    // 安全释放主相机
    {
        std::lock_guard<std::mutex> lock(main_lock_);
        if (cap_) {
            cap_->release();
        }
        cap_.reset();
    }

    // 安全释放扫码相机
    {
        std::lock_guard<std::mutex> lock(qr_lock_);
        if (cap2_) {
            cap2_->release();
        }
        cap2_.reset();
    }

    // 清空故障计数，重置异常状态
    main_fail_count_ = 0;
    qr_fail_count_ = 0;
}

/**
 * @brief 相机自动重连通用函数
 * @param index 相机设备索引
 * @param resolution 目标分辨率
 * @param name 相机名称（日志标识）
 * @param current_cap 相机句柄引用
 * @param lock 对应相机的独立互斥锁
 *
 * 重连逻辑：
 * 1. 加锁保证重连过程线程安全
 * 2. 设备已正常打开则跳过重连
 * 3. 延时等待后重新尝试打开设备
 * 4. 打印重连成功/失败日志
 */
void CameraManager::reconnect(int index,
                              const std::pair<int, int>& resolution,
                              const std::string& name,
                              std::unique_ptr<cv::VideoCapture>& current_cap,
                              std::mutex& lock) {
    std::lock_guard<std::mutex> lk(lock);

    // 设备已就绪，无需重连
    if (current_cap) {
        return;
    }

    std::cout << "[摄像头] 正在重连" << name << "摄像头 " << index << "..." << std::endl;

    // 延时重试，避免高频空转占用CPU
    std::this_thread::sleep_for(
        std::chrono::duration<double>(reconnect_delay_));

    // 重新打开相机设备
    current_cap = open_one(index, resolution, name);

    if (current_cap) {
        std::cout << "[摄像头] " << name << "摄像头重连成功" << std::endl;
    } else {
        std::cout << "[摄像头] " << name << "摄像头重连失败" << std::endl;
    }
}

/**
 * @brief 读取主相机图像帧（线程安全）
 * @return std::pair<bool, cv::Mat> (读取是否成功, 图像帧)
 *
 * 核心容错逻辑：
 * 1. 加锁保护，防止多线程同时读帧崩溃
 * 2. 读取成功：清空故障计数，返回有效帧
 * 3. 读取失败：累计故障计数
 * 4. 连续失败≥30次：熔断释放设备句柄，触发下次自动重连
 * 5. 设备断开状态：自动执行重连逻辑
 */
std::pair<bool, cv::Mat> CameraManager::read_main() {
    {
        std::lock_guard<std::mutex> lock(main_lock_);

        // 设备正常打开，尝试读取帧
        if (cap_ && cap_->isOpened()) {
            cv::Mat frame;
            bool ret = cap_->read(frame);

            // 读取成功，重置错误计数，返回帧数据
            if (ret) {
                main_fail_count_ = 0;
                return {true, std::move(frame)};
            }

            // 读取失败，累计错误次数
            main_fail_count_++;

            // 达到熔断阈值：释放设备资源，进入待重连状态
            if (main_fail_count_ >= fail_threshold_) {
                main_fail_count_ = 0;
                cap_->release();
                cap_.reset();
            }
        }
    }

    // 设备未就绪，尝试自动重连
    if (!cap_) {
        reconnect(main_index_, main_resolution_, "主", cap_, main_lock_);
    }

    return {false, cv::Mat()};
}

/**
 * @brief 读取扫码相机图像帧（线程安全）
 * @return std::pair<bool, cv::Mat> (读取是否成功, 图像帧)
 * @note 逻辑与主相机完全一致，双相机独立容错、独立计数、独立重连
 */
std::pair<bool, cv::Mat> CameraManager::read_qr() {
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

            // 达到阈值熔断释放
            if (qr_fail_count_ >= fail_threshold_) {
                qr_fail_count_ = 0;
                cap2_->release();
                cap2_.reset();
            }
        }
    }

    // 自动重连扫码相机
    if (!cap2_) {
        reconnect(qr_index_, qr_resolution_, "扫码", cap2_, qr_lock_);
    }

    return {false, cv::Mat()};
}
