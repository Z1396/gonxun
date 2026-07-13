/**
 * 视觉系统控制器
 * 管理 VisionWorker 线程的启动、停止和资源回收
 */
#pragma once

#include <QObject>
#include <QThread>
#include <atomic>

class VisionSystem;

namespace gonxun {

/**
 * 视觉工作线程
 * 在独立线程中运行视觉处理循环
 */
class VisionWorker : public QObject {
    Q_OBJECT
public:
    VisionWorker(VisionSystem* vs, std::atomic<bool>* running);

public slots:
    void run();

signals:
    void finished();

private:
    VisionSystem* m_visionSystem;
    std::atomic<bool>* m_running;
};

/**
 * 视觉系统控制器
 * 解耦 UI 和视觉线程，统一管理启停
 */
class VisionController : public QObject {
    Q_OBJECT
public:
    explicit VisionController(VisionSystem* vs);
    ~VisionController();

public slots:
    void start();
    void stop();

private slots:
    void onFinished();

private:
    VisionSystem* m_visionSystem;
    VisionWorker* m_worker;
    QThread* m_thread;
    std::atomic<bool> m_running;
};

} // namespace gonxun