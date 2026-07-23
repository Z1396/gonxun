/**
 * @file vision_controller.cpp
 * @brief 视觉系统控制器实现。
 *
 * 架构分层：
 *  1. VisionWorker：工作线程业务体，死循环执行相机读取 + 图像算法处理
 *  2. VisionController：线程管理层，对外提供 start/stop 接口，管控线程生命周期
 *
 * 核心机制：
 *  - 独立子线程运行视觉算法，杜绝UI卡顿
 *  - 双层原子布尔守卫（局部running_ + 全局g_running）保证线程安全退出
 *  - Qt信号槽全自动资源回收，零内存泄漏
 *  - 完全解耦：UI层只调用控制器接口，不碰线程与算法底层
 */

#include "vision_controller.hpp"
#include "vision_system.hpp"

// Qt线程核心库
#include <QThread>
// 控制台日志输出
#include <iostream>

namespace gonxun {

/**
 * @brief 视觉工作器构造函数
 * @param vs 全局视觉系统实例（包含相机、算法、YOLO、滤波）
 * @param running 控制器层运行状态原子变量（线程启停开关）
 * @note 接收外部原子布尔指针，实现跨类、跨线程安全状态控制
 */
VisionWorker::VisionWorker(VisionSystem* vs, std::atomic<bool>* running)
    : vision_system_(vs), running_(running) {}

/**
 * @brief 视觉工作线程主循环（子线程执行体）
 * @details 运行在独立子线程，严禁操作UI控件
 * 线程退出双条件机制（必须同时满足）：
 *  1. 控制器启停标记 running_ = false（主动停止）
 *  2. 全局系统运行标记 g_running = false（程序退出）
 *
 * 业务流程：
 *  1. 循环读取主相机图像帧
 *  2. 读取成功：执行全套视觉算法处理
 *  3. 读取失败：短暂休眠防CPU满载空转
 *  4. 退出循环后发射结束信号，触发资源自动回收
 */
void VisionWorker::run() {
    std::cout << "[VisionWorker] 视觉系统线程启动" << std::endl;

    // 引入全局原子运行标记（系统级优雅退出开关）
    extern std::atomic<bool> g_running;

    // ========== 双层线程安全死循环 ==========
    // *running_：外部控制器启停控制
    // g_running：全局程序退出控制（信号触发）
    while (*running_ && g_running) {
        // 读取主相机原始图像帧
        auto [success, frame] = vision_system_->camera.read_main();

        if (success) {
            // 帧读取成功：执行完整视觉处理流水线
            // 包含畸变校正、YOLO识别、卡尔曼滤波、坐标解算
            cv::Mat result = vision_system_->process_frame(frame, -1);
        } else {
            // 相机读取失败/无帧：休眠10ms，降低CPU占用
            // 避免设备未就绪、断连时死循环空跑
            QThread::msleep(10);
        }
    }

    // 跳出循环，线程即将结束
    std::cout << "[VisionWorker] 视觉系统线程退出" << std::endl;
    // 发射线程结束信号，触发后续资源清理链式回调
    emit finished();
}

/**
 * @brief 视觉控制器构造函数
 * @param vs 全局视觉系统业务实例
 * @note 初始化空指针，延迟创建线程与Worker对象
 * @note running_默认false：默认未启动状态
 */
VisionController::VisionController(VisionSystem* vs)
    : vision_system_(vs), worker_(nullptr), thread_(nullptr), running_(false) {}

/**
 * @brief 视觉控制器析构函数
 * @note 对象销毁前强制停止线程，杜绝野线程、内存泄漏
 * 保证程序退出时视觉线程一定安全终止
 */
VisionController::~VisionController() {
    stop();
}

/**
 * @brief 启动视觉采集与算法线程（对外接口）
 * @details 标准Qt安全子线程创建流程（无内存泄漏工业级写法）
 * 执行流程：
 *  1. 防重入判断，避免重复创建线程
 *  2. 新建线程容器 + 业务Worker
 *  3. 将Worker移动至新线程（核心：对象依附子线程）
 *  4. 绑定全套生命周期信号槽
 *  5. 启动子线程，开始循环推理
 */
void VisionController::start() {
    // 防重入：已运行直接返回，防止重复创建线程
    if (running_) return;
    running_ = true;

    // 1. 创建线程容器、工作器对象
    thread_ = new QThread;
    worker_ = new VisionWorker(vision_system_, &running_);

    // 2. 核心：将Worker对象移动到新线程执行
    // Qt规则：所有耗时业务必须在所属线程执行，禁止跨线程调用
    worker_->moveToThread(thread_);

    // ========== 生命周期信号槽绑定（全自动回收） ==========
    // 线程启动后，执行Worker主循环
    connect(thread_, &QThread::started, worker_, &VisionWorker::run);
    // Worker执行完毕，触发控制器收尾
    connect(worker_, &VisionWorker::finished, this, &VisionController::on_finished);
    // Worker结束，退出线程事件循环
    connect(worker_, &VisionWorker::finished, thread_, &QThread::quit);
    // Worker结束，自动释放Worker内存
    connect(worker_, &VisionWorker::finished, worker_, &QObject::deleteLater);
    // 线程退出后，自动释放线程内存
    connect(thread_, &QThread::finished, thread_, &QObject::deleteLater);

    // 3. 启动子线程
    thread_->start();
    std::cout << "[VisionController] 视觉系统已启动" << std::endl;
}

/**
 * @brief 停止视觉线程（对外安全停机接口）
 * @details 主动停机逻辑：
 *  1. 关闭运行开关，让Worker循环自然退出
 *  2. 主动退出线程并阻塞等待结束
 *  3. 置空指针，防止野指针访问
 */
void VisionController::stop() {
    // 未运行无需停止
    if (!running_) return;
    running_ = false;

    // 安全等待线程退出，防止强制终止导致资源错乱
    if (thread_ && thread_->isRunning()) {
        thread_->quit();    // 请求线程退出事件循环
        thread_->wait();    // 阻塞主线程，直到子线程完全结束（安全停机）
    }

    // 手动置空指针，配合deleteLater完成双重安全释放
    worker_ = nullptr;
    thread_ = nullptr;
    std::cout << "[VisionController] 视觉系统已停止" << std::endl;
}

/**
 * @brief 线程结束收尾回调
 * @note 由Worker::finished信号触发
 * 重置运行状态、清空指针，保证下次start可正常启动
 */
void VisionController::on_finished() {
    running_ = false;
    worker_ = nullptr;
    thread_ = nullptr;
}

} // namespace gonxun
