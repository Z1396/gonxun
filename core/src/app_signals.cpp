/**
 * @file app_signals.cpp
 * @brief 应用程序信号处理实现文件
 * 
 * @details 本文件实现了应用程序的信号处理机制，用于优雅地处理外部中断信号。
 *          核心功能：
 *          - 信号捕获：捕获 SIGINT（Ctrl+C）和 SIGTERM（终止信号）
 *          - 全局运行标志：提供线程安全的运行状态控制
 *          - Qt 集成：与 QCoreApplication 的事件循环集成
 *          - 优雅退出：确保资源正确释放和程序正常退出
 * 
 * @author 智能物流搬运系统开发团队
 * @version 1.0
 * @date 2025-01-01
 * 
 * @note 修改历史：
 *       - 2025-01-01: 初始版本
 *       - 2025-02-15: 增加 Qt 事件循环集成
 *       
 * @note 使用示例：
 *       ```cpp
 *       int main(int argc, char* argv[]) {
 *           QCoreApplication app(argc, argv);
 *           gonxun::setupSignalHandlers();
 *           
 *           while (gonxun::isRunning()) {
 *               // 执行主循环任务
 *               std::this_thread::sleep_for(std::chrono::milliseconds(100));
 *           }
 *           
 *           return 0;
 *       }
 *       ```
 *       
 * @note 线程安全设计：
 *       - 使用 std::atomic<bool> 确保多线程安全访问
 *       - 全局变量 g_running 可被多个线程安全读取
 *       - requestExit() 可从任意线程调用
 *       
 * @see app_signals.hpp
 */
#include "app_signals.hpp"
#include <iostream>
#include <csignal>
#include <QCoreApplication>

namespace gonxun {

/**
 * @brief 全局运行标志（原子变量）
 * 
 * @details 用于控制整个应用程序的运行状态。
 *          - 初始值：true（应用程序启动后立即开始运行）
 *          - 当收到外部信号时，被设置为 false
 *          - 所有工作线程应定期检查此标志以响应退出请求
 *          
 * @note 线程安全：
 *       - std::atomic 确保多线程访问的原子性
 *       - 读取操作（isRunning()）不会产生数据竞争
 *       - 写入操作（requestExit()）对所有线程可见
 */
std::atomic<bool> g_running{true};

/**
 * @brief 信号处理函数
 * 
 * @details 当收到 SIGINT 或 SIGTERM 信号时调用此函数。
 *          执行以下操作：
 *          1. 输出提示信息
 *          2. 设置全局运行标志为 false
 *          3. 调用 QCoreApplication::quit() 退出事件循环
 *          
 * @param signal 接收到的信号值
 *        - SIGINT (2): 通常由 Ctrl+C 触发
 *        - SIGTERM (15): 通常由 kill 命令触发
 *        
 * @note 静态函数：
 *       - 使用 static 限定符，仅在当前文件可见
 *       - 符合 POSIX 信号处理函数的签名要求
 *       
 * @warning 信号处理函数中的操作应尽量简单，避免调用非异步信号安全函数
 */
static void signalHandler(int signal) {
    std::cout << "\n收到信号 " << signal << "，正在退出..." << std::endl;
    g_running = false;                // 设置全局退出标志
    QCoreApplication::quit();         // 退出 Qt 事件循环
}

/**
 * @brief 设置信号处理器
 * 
 * @details 注册 SIGINT 和 SIGTERM 信号的处理函数。
 *          应在应用程序启动时（main 函数开始处）调用。
 *          
 * @note 使用方法：
 *       ```cpp
 *       int main(int argc, char* argv[]) {
 *           QCoreApplication app(argc, argv);
 *           gonxun::setupSignalHandlers();  // 必须在 app 创建后调用
 *           // ...
 *       }
 *       ```
 *       
 * @note 信号说明：
 *       - SIGINT (2): 中断信号，通常由 Ctrl+C 触发
 *       - SIGTERM (15): 终止信号，通常由 kill 命令发送
 *       
 * @see signalHandler()
 */
void setupSignalHandlers() {
    std::signal(SIGINT, signalHandler);    // 注册 SIGINT 处理函数
    std::signal(SIGTERM, signalHandler);   // 注册 SIGTERM 处理函数
}

/**
 * @brief 检查应用程序是否正在运行
 * 
 * @details 读取全局运行标志，判断应用程序是否应该继续运行。
 *          工作线程应定期调用此函数以响应退出请求。
 *          
 * @return true 应用程序正在运行，应继续执行任务
 * @return false 应用程序已收到退出信号，应停止执行
 *         
 * @note 线程安全：
 *       - std::atomic 的读取操作是线程安全的
 *       - 无需额外同步机制
 *       
 * @note 使用示例：
 *       ```cpp
 *       while (gonxun::isRunning()) {
 *           // 执行任务
 *           processTask();
 *       }
 *       ```
 */
bool isRunning() {
    return g_running.load();  // 原子读取运行标志
}

/**
 * @brief 请求应用程序退出
 * 
 * @details 设置全局运行标志为 false，通知所有线程停止运行。
 *          可从任意线程调用，用于程序内部控制退出流程。
 *          
 * @note 与信号处理的区别：
 *       - setupSignalHandlers() 用于响应外部信号（Ctrl+C, kill）
 *       - requestExit() 用于程序内部控制退出（如异常、任务完成）
 *       
 * @note 线程安全：
 *       - std::atomic 的写入操作是线程安全的
 *       - 所有正在检查 isRunning() 的线程将立即感知到变化
 *       
 * @note 使用示例：
 *       ```cpp
 *       void onTaskCompleted() {
 *           gonxun::requestExit();  // 任务完成后请求退出
 *       }
 *       ```
 */
void requestExit() {
    g_running.store(false);  // 原子设置运行标志为 false
}

} // namespace gonxun