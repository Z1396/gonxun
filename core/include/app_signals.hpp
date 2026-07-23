/**
 * @file app_signals.hpp
 * @brief 应用程序信号处理，捕获 SIGINT/SIGTERM 实现优雅退出。
 *
 * 提供全局原子运行标志 g_running、信号处理器安装函数、
 * 运行状态查询和主动退出请求接口，供各子系统在主循环中轮询。
 */

#pragma once

#include <atomic>

namespace gonxun {

/// 全局运行标志，true 表示程序正常运行，false 表示请求退出
extern std::atomic<bool> g_running;

/**
 * @brief 安装 SIGINT/SIGTERM 信号处理器。
 *
 * 收到信号后将 g_running 置 false 并调用 QCoreApplication::quit()。
 */
void setup_signal_handlers();

/**
 * @brief 查询程序是否仍在运行。
 * @return true 表示正常运行
 */
[[nodiscard]] bool is_running();

/**
 * @brief 请求程序退出，将 g_running 置 false。
 * @note 线程安全，可从任意线程调用
 */
void request_exit();

} // namespace gonxun
