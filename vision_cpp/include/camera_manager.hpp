/**
 * @file camera_manager.hpp
 * @brief 双摄像头管理器：主摄像头 + 扫码摄像头，支持自动重连和分辨率降级
 *
 * 管理两个独立的 cv::VideoCapture 实例，分别用于主视觉和 QR 码扫描。
 * 特性:
 * - 平台自适应后端: Linux 使用 V4L2，Windows 使用 DSHOW
 * - 分辨率降级: 请求分辨率不可用时自动降级
 * - 自动重连: 连续读取失败超过阈值后释放并重试打开
 * - 线程安全: 每个摄像头有独立的 mutex 保护
 */
#pragma once

#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>
#include <opencv2/opencv.hpp>

/**
 * @brief 分辨率降级序列，从高到低尝试
 */
extern const std::vector<std::pair<int, int>> FALLBACK_RESOLUTIONS;

/**
 * @brief 双摄像头管理器
 */
class CameraManager {
public:
    /**
     * @brief 构造函数
     * @param main_index 主摄像头设备索引
     * @param qr_index 扫码摄像头设备索引
     * @param main_width 主摄像头请求宽度 (px)
     * @param main_height 主摄像头请求高度 (px)
     * @param qr_width 扫码摄像头请求宽度 (px)
     * @param qr_height 扫码摄像头请求高度 (px)
     * @param max_reconnect 最大重连次数，默认 3
     * @param reconnect_delay 重连等待时间 (秒)，默认 1.0
     */
    explicit CameraManager(int main_index = 1, int qr_index = 2,
                           int main_width = 640, int main_height = 480,
                           int qr_width = 640, int qr_height = 480,
                           int max_reconnect = 3, double reconnect_delay = 1.0);
    ~CameraManager();

    CameraManager(const CameraManager&) = delete;
    CameraManager& operator=(const CameraManager&) = delete;

    /** @brief 打开双摄像头 */
    void open();
    /** @brief 关闭并释放双摄像头资源 */
    void close();

    /**
     * @brief 读取主摄像头帧
     * @return (是否成功, 帧图像)；失败时图像为空
     * @note 连续失败超过阈值会触发自动重连
     */
    [[nodiscard]] std::pair<bool, cv::Mat> read_main();

    /**
     * @brief 读取扫码摄像头帧
     * @return (是否成功, 帧图像)；失败时图像为空
     */
    [[nodiscard]] std::pair<bool, cv::Mat> read_qr();

private:
    /**
     * @brief 获取平台首选摄像头后端
     * @return Linux 返回 CAP_V4L2，Windows 返回 CAP_DSHOW，其他 CAP_ANY
     */
    [[nodiscard]] static int get_preferred_backend();

    /**
     * @brief 配置摄像头分辨率和曝光参数
     * @param cap 已打开的 VideoCapture
     * @param resolution 目标分辨率 (width, height)
     */
    void configure_capture(cv::VideoCapture& cap,
                           const std::pair<int, int>& resolution) const;

    /**
     * @brief 打开单个摄像头，尝试首选后端和默认后端
     * @param index 设备索引
     * @param resolution 目标分辨率
     * @param name 摄像头名称（用于日志）
     * @return VideoCapture 智能指针；失败返回 nullptr
     */
    [[nodiscard]] std::unique_ptr<cv::VideoCapture> open_one(int index,
                                                             const std::pair<int, int>& resolution,
                                                             const std::string& name) const;

    /**
     * @brief 重连摄像头
     * @param index 设备索引
     * @param resolution 目标分辨率
     * @param name 名称
     * @param current_cap 当前 capture 智能指针引用
     * @param lock 对应互斥锁引用
     */
    void reconnect(int index,
                   const std::pair<int, int>& resolution,
                   const std::string& name,
                   std::unique_ptr<cv::VideoCapture>& current_cap,
                   std::mutex& lock);

    int main_index_;              ///< 主摄像头设备索引
    int qr_index_;                ///< 扫码摄像头设备索引
    std::pair<int, int> main_resolution_; ///< 主摄像头目标分辨率
    std::pair<int, int> qr_resolution_;   ///< 扫码摄像头目标分辨率
    int max_reconnect_;           ///< 最大重连次数
    double reconnect_delay_;      ///< 重连等待时间 (秒)
    int backend_;                 ///< 首选后端

    std::unique_ptr<cv::VideoCapture> cap_;  ///< 主摄像头
    std::unique_ptr<cv::VideoCapture> cap2_; ///< 扫码摄像头
    std::mutex main_lock_;  ///< 主摄像头互斥锁
    std::mutex qr_lock_;    ///< 扫码摄像头互斥锁

    int main_fail_count_;   ///< 主摄像头连续失败计数
    int qr_fail_count_;     ///< 扫码摄像头连续失败计数
    int fail_threshold_;    ///< 触发重连的失败次数阈值
};
