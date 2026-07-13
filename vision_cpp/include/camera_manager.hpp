// 摄像头管理模块头文件
// - 主摄像头 (物料/色环识别)
// - 扫码摄像头 (二维码识别)
// 使用互斥锁保护 VideoCapture，支持自动重连、跨平台后端选择、降级分辨率
#pragma once

#include <memory>
#include <mutex>
#include <opencv2/opencv.hpp>
#include <string>
#include <utility>
#include <vector>

// 降级分辨率列表（宽, 高）
extern const std::vector<std::pair<int, int>> FALLBACK_RESOLUTIONS;

// 双摄像头管理器：主摄像头 + 扫码摄像头
class CameraManager {
public:
    CameraManager(int main_index = 1, int qr_index = 2,
                  int main_width = 640, int main_height = 480,
                  int qr_width = 640, int qr_height = 480,
                  int max_reconnect = 3, double reconnect_delay = 1.0);
    ~CameraManager();

    // 禁止拷贝（持有 VideoCapture 与互斥锁）
    CameraManager(const CameraManager&) = delete;
    CameraManager& operator=(const CameraManager&) = delete;

    // 打开主摄像头和扫码摄像头
    void open();
    // 关闭所有摄像头
    void close();
    // 读取主摄像头帧，支持自动重连；返回 (是否成功, 帧图像)
    std::pair<bool, cv::Mat> readMain();
    // 读取扫码摄像头帧，支持自动重连；返回 (是否成功, 帧图像)
    std::pair<bool, cv::Mat> readQr();

private:
    // 根据操作系统选择最优后端
    static int getPreferredBackend();
    // 配置摄像头分辨率与缓冲区大小
    void configureCapture(cv::VideoCapture& cap,
                          const std::pair<int, int>& resolution) const;
    // 打开单个摄像头，支持多后端尝试和分辨率降级
    std::unique_ptr<cv::VideoCapture> openOne(int index,
                                              const std::pair<int, int>& resolution,
                                              const std::string& name) const;
    // 重连摄像头（结果通过 current_cap 引用回传）
    void reconnect(int index,
                   const std::pair<int, int>& resolution,
                   const std::string& name,
                   std::unique_ptr<cv::VideoCapture>& current_cap,
                   std::mutex& lock);

    int main_index_;
    int qr_index_;
    std::pair<int, int> main_resolution_;
    std::pair<int, int> qr_resolution_;
    int max_reconnect_;
    double reconnect_delay_;
    int backend_;

    std::unique_ptr<cv::VideoCapture> cap_;
    std::unique_ptr<cv::VideoCapture> cap2_;
    std::mutex main_lock_;
    std::mutex qr_lock_;

    int main_fail_count_;
    int qr_fail_count_;
    int fail_threshold_;
};
