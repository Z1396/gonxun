/**
 * @file vision_controller.cpp
 * @brief 视觉系统控制器实现文件
 * 
 * @details 本文件实现了视觉系统的线程管理和控制功能。
 *          核心特性：
 *          - QThread 线程管理：使用 Qt 的线程框架
 *          - 工作者模式：VisionWorker 在独立线程中运行
 *          - 信号槽连接：使用 Qt 的信号槽机制通信
 *          - 优雅退出：正确处理线程停止和资源释放
 *          - 全局状态同步：与 g_running 标志集成
 * 
 * @author 智能物流搬运系统开发团队
 * @version 1.0
 * @date 2025-01-01
 * 
 * @note 修改历史：
 *       - 2025-01-01: 初始版本，实现基础线程控制
 *       - 2025-02-15: 增加全局运行标志集成
 *       
 * @note Qt 线程架构：
 *       - VisionController: 控制器，管理线程生命周期
 *       - VisionWorker: 工作者，在独立线程中执行视觉处理
 *       - QThread: 线程对象，提供事件循环
 *       - 信号槽: 用于线程间通信
 *       
 * @note 线程安全设计：
 *       - 使用 std::atomic<bool> 控制运行状态
 *       - 避免数据竞争：每个线程有独立的栈空间
 *       - 跨线程通信：使用 Qt 的信号槽机制（线程安全）
 *       
 * @note 生命周期管理：
 *       1. start() 创建 QThread 和 VisionWorker
 *       2. moveToThread() 将 Worker 移动到新线程
 *       3. 连接信号槽（started → run, finished → quit）
 *       4. stop() 设置运行标志为 false，等待线程退出
 *       5. 析构时自动停止线程并释放资源
 *       
 * @warning 不要在主线程中直接调用 VisionSystem 的方法！
 *          所有视觉处理必须在 Worker 线程中执行。
 *          
 * @see vision_controller.hpp
 */
#include "vision_controller.hpp"
#include "vision_system.hpp"
#include <iostream>
#include <QThread>

namespace gonxun {

// ========== VisionWorker 实现 ==========

/**
 * @brief VisionWorker 构造函数
 * 
 * @details 初始化工作者对象，保存视觉系统指针和运行标志。
 *          
 * @param vs 视觉系统对象指针
 * @param running 运行标志指针（std::atomic<bool>）
 */
VisionWorker::VisionWorker(VisionSystem* vs, std::atomic<bool>* running)
    : m_visionSystem(vs), m_running(running) {}

/**
 * @brief 工作者主函数，在独立线程中运行
 * 
 * @details 循环读取摄像头图像，执行视觉处理，直到运行标志为 false。
 *          
 * @note 执行流程：
 *       1. 读取摄像头图像
 *       2. 调用 VisionSystem::processFrame() 处理
 *       3. 发送结果到 GUI（TODO: 待实现）
 *       4. 如果读取失败，休眠 10ms
 *       5. 循环直到运行标志为 false
 *       
 * @note 线程同步：
 *       - 检查两个运行标志：m_running（本地）和 g_running（全局）
 *       - 任一标志为 false 都会导致线程退出
 *       
 * @warning 不要在主线程中直接调用此函数！
 *          它必须在独立的 QThread 中执行。
 */
void VisionWorker::run() {
    std::cout << "[VisionWorker] 视觉系统线程启动" << std::endl;

    // 外部运行标志（由 main.cpp 提供的全局退出信号）
    extern std::atomic<bool> g_running;

    // 主循环：直到运行标志为 false
    while (*m_running && g_running) {
        // 步骤 1: 读取摄像头图像
        auto [success, frame] = m_visionSystem->camera.readMain();

        if (success) {
            // 步骤 2: 处理图像
            cv::Mat result = m_visionSystem->processFrame(frame);
            
            // 步骤 3: 发送结果到 GUI（TODO: 待实现）
            // emit resultReady(result);
        } else {
            // 读取失败，休眠 10ms 避免 CPU 占用过高
            QThread::msleep(10);
        }
    }

    std::cout << "[VisionWorker] 视觉系统线程退出" << std::endl;
    
    // 发送完成信号
    emit finished();
}

// ========== VisionController 实现 ==========

VisionController::VisionController(VisionSystem* vs)
    : m_visionSystem(vs), m_worker(nullptr), m_thread(nullptr), m_running(false) {}

VisionController::~VisionController() {
    stop();
}

/**
 * @brief 启动视觉系统线程
 * 
 * @details 创建 QThread 和 VisionWorker，并启动线程。
 *          
 * @note 启动流程：
 *       1. 检查是否已启动（避免重复启动）
 *       2. 创建 QThread 对象
 *       3. 创建 VisionWorker 对象
 *       4. 将 Worker 移动到新线程
 *       5. 连接信号槽（生命周期管理）
 *       6. 启动线程
 *       
 * @note 信号槽连接：
 *       - thread::started → worker::run: 线程启动时开始运行
 *       - worker::finished → this::onFinished: Worker 完成时通知控制器
 *       - worker::finished → thread::quit: Worker 完成时退出线程事件循环
 *       - worker::finished → worker::deleteLater: Worker 完成时自动删除
 *       - thread::finished → thread::deleteLater: 线程完成时自动删除
 *       
 * @warning 此函数只能在主线程中调用！
 */
void VisionController::start() {
    // 检查是否已启动
    if (m_running) return;

    m_running = true;

    // 创建线程对象
    m_thread = new QThread;
    
    // 创建工作者对象
    m_worker = new VisionWorker(m_visionSystem, &m_running);
    
    // 将 Worker 移动到新线程
    m_worker->moveToThread(m_thread);

    // 连接信号槽（生命周期管理）
    connect(m_thread, &QThread::started, m_worker, &VisionWorker::run);
    connect(m_worker, &VisionWorker::finished, this, &VisionController::onFinished);
    connect(m_worker, &VisionWorker::finished, m_thread, &QThread::quit);
    connect(m_worker, &VisionWorker::finished, m_worker, &QObject::deleteLater);
    connect(m_thread, &QThread::finished, m_thread, &QObject::deleteLater);

    // 启动线程
    m_thread->start();
    std::cout << "[VisionController] 视觉系统已启动" << std::endl;
}

/**
 * @brief 停止视觉系统线程
 * 
 * @details 设置运行标志为 false，并等待线程退出。
 *          
 * @note 停止流程：
 *       1. 检查是否已停止（避免重复停止）
 *       2. 设置运行标志为 false
 *       3. 退出线程事件循环（quit）
 *       4. 等待线程结束（wait）
 *       5. 清空指针（对象已由 deleteLater 自动删除）
 *       
 * @warning 此函数会阻塞当前线程，直到视觉线程退出。
 *          如果视觉线程正在处理图像，可能需要等待较长时间。
 */
void VisionController::stop() {
    // 检查是否已停止
    if (!m_running) return;

    // 设置运行标志为 false
    m_running = false;

    // 等待线程退出
    if (m_thread && m_thread->isRunning()) {
        m_thread->quit();   // 退出事件循环
        m_thread->wait();   // 等待线程结束
    }

    // 清空指针（对象已由 deleteLater 自动删除）
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