/**
 * 视觉系统控制器实现
 */

#include "vision_controller.hpp"
#include "vision_system.hpp"
#include <iostream>
#include <QThread>

namespace gonxun {

// ========== VisionWorker 实现 ==========

VisionWorker::VisionWorker(VisionSystem* vs, std::atomic<bool>* running)
    : m_visionSystem(vs), m_running(running) {}

void VisionWorker::run() {
    std::cout << "[VisionWorker] 视觉系统线程启动" << std::endl;

    // 外部运行标志（由 main.cpp 提供的全局退出信号）
    extern std::atomic<bool> g_running;

    while (*m_running && g_running) {
        auto [success, frame] = m_visionSystem->camera.readMain();

        if (success) {
            cv::Mat result = m_visionSystem->processFrame(frame);
            // TODO: 发送结果到 GUI
        } else {
            QThread::msleep(10);
        }
    }

    std::cout << "[VisionWorker] 视觉系统线程退出" << std::endl;
    emit finished();
}

// ========== VisionController 实现 ==========

VisionController::VisionController(VisionSystem* vs)
    : m_visionSystem(vs), m_worker(nullptr), m_thread(nullptr), m_running(false) {}

VisionController::~VisionController() {
    stop();
}

void VisionController::start() {
    if (m_running) return;

    m_running = true;

    m_thread = new QThread;
    m_worker = new VisionWorker(m_visionSystem, &m_running);
    m_worker->moveToThread(m_thread);

    connect(m_thread, &QThread::started, m_worker, &VisionWorker::run);
    connect(m_worker, &VisionWorker::finished, this, &VisionController::onFinished);
    connect(m_worker, &VisionWorker::finished, m_thread, &QThread::quit);
    connect(m_worker, &VisionWorker::finished, m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);

    m_thread->start();
    std::cout << "[VisionController] 视觉系统已启动" << std::endl;
}

void VisionController::stop() {
    if (!m_running) return;

    m_running = false;

    if (m_thread && m_thread->isRunning()) {
        m_thread->quit();
        m_thread->wait();
    }

    m_worker = nullptr;
    m_thread = nullptr;
    std::cout << "[VisionController] 视觉系统已停止" << std::endl;
}

void VisionController::onFinished() {
    m_running = false;
    m_worker = nullptr;
    m_thread = nullptr;
}

} // namespace gonxun