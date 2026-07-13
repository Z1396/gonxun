/**
 * 应用程序信号处理
 * 捕获系统信号实现优雅退出
 */
#pragma once

#include <atomic>

namespace gonxun {

/**
 * 全局运行标志
 * 控制所有线程和循环的启停
 */
extern std::atomic<bool> g_running;

/**
 * 初始化信号处理器
 * 捕获 SIGINT (Ctrl+C) 和 SIGTERM
 */
void setupSignalHandlers();

/**
 * 获取全局运行状态
 */
bool isRunning();

/**
 * 请求程序退出
 */
void requestExit();

} // namespace gonxun