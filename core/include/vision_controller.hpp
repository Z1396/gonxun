/**
 * @file vision_controller.hpp
 * @brief 视觉系统控制器，管理 VisionWorker 线程的启停。
 *
 * 将 VisionSystem 的图像采集与处理循环移至独立 QThread，
 * 通过 Qt 信号槽机制控制线程生命周期，实现视觉子系统的
 * 异步运行与优雅退出。
 */

#pragma once

#include <QThread>
#include <QImage>
#include <atomic>
#include <QObject>
#include <opencv2/opencv.hpp>

class VisionSystem;

namespace gonxun {

/**
 * @brief 视觉工作线程对象，在独立 QThread 中执行图像采集处理循环。
 *
 * 循环调用 VisionSystem::camera.read_main() 读取主相机帧，
 * 再调用 VisionSystem::process_frame() 进行处理，
 * 当 running_ 标志或全局 g_running 为 false 时退出。
 */
class VisionWorker : public QObject {
    Q_OBJECT
public:
    /**
     * @brief 构造 VisionWorker。
     * @param vs VisionSystem 实例指针
     * @param running 运行标志原子变量指针
     */
    explicit VisionWorker(VisionSystem* vs, std::atomic<bool>* running);

public slots:
    /// 线程入口，执行视觉采集处理循环
    void run();

signals:
    /// 工作线程正常退出时发射
    void finished();
    /// 主摄像头图像帧就绪
    void frame_ready(const QImage& frame);
    /// 扫码摄像头图像帧就绪
    void qr_frame_ready(const QImage& frame);

private:
    VisionSystem* vision_system_;     ///< 视觉系统实例
    std::atomic<bool>* running_;      ///< 运行标志（外部持有）
};

/**
 * @brief 视觉系统控制器，负责创建和管理 VisionWorker 线程。
 *
 * 调用 start() 启动视觉线程，调用 stop() 请求退出并等待线程结束。
 * 析构时自动停止线程。线程安全：running_ 为 atomic<bool>。
 */
class VisionController : public QObject {
    Q_OBJECT
public:
    /**
     * @brief 构造视觉控制器。
     * @param vs VisionSystem 实例指针
     */
    explicit VisionController(VisionSystem* vs);
    /// 析构时自动停止视觉线程
    ~VisionController() override;

public slots:
    /// 启动视觉采集处理线程
    void start();
    /// 停止视觉线程并等待退出
    void stop();

signals:
    /// 主摄像头图像帧就绪（转发自 VisionWorker）
    void frame_ready(const QImage& frame);
    /// 扫码摄像头图像帧就绪（转发自 VisionWorker）
    void qr_frame_ready(const QImage& frame);

private slots:
    /// 工作线程完成时的清理回调
    void on_finished();

private:
    VisionSystem* vision_system_;     ///< 视觉系统实例
    VisionWorker* worker_;            ///< 工作线程对象
    QThread* thread_;                 ///< 承载 worker 的 QThread
    std::atomic<bool> running_;       ///< 运行标志
};

} // namespace gonxun
