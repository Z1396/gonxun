/**
 * 应用程序信号处理实现
 */

#include "app_signals.hpp"
#include <iostream>
#include <csignal>
#include <QCoreApplication>

namespace gonxun {

// 全局运行标志定义
std::atomic<bool> g_running{true};

// 信号处理函数
static void signalHandler(int signal) {
    std::cout << "\n收到信号 " << signal << "，正在退出..." << std::endl;
    g_running = false;
    QCoreApplication::quit();
}

void setupSignalHandlers() {
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
}

bool isRunning() {
    return g_running;
}

void requestExit() {
    g_running = false;
}

} // namespace gonxun