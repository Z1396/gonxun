/**
 * @file logger.hpp
 * @brief 通用日志系统，RAII 生命周期管理。
 *
 * 程序启动时自动清空旧日志，每次运行只保留当前会话的日志记录。
 * 支持多日志级别：INFO、WARN、ERROR。
 * 自动重定向 stdout/stderr 到日志文件，捕获所有终端输出。
 */

#pragma once

#include "common_types.hpp"

#include <cstdio>
#include <iostream>
#include <mutex>
#include <string>

namespace gonxun {

// ==== 日志级别 ====

/// 日志级别枚举
enum class LogLevel {
    INFO,   ///< 普通信息
    WARN,   ///< 警告信息
    ERROR   ///< 错误信息
};

// ==== 日志系统（单例 + RAII） ====

/**
 * @brief 通用日志系统，单例模式，RAII 生命周期管理，线程安全。
 *
 * 特性：
 * - 程序启动时自动清空旧日志文件
 * - 每次运行只保留当前会话的日志
 * - 线程安全（内部互斥锁保护）
 * - 自动添加时间戳和日志级别
 * - 自动捕获所有 stdout/stderr 输出到日志文件
 * - 使用 FileOwner 管理 FILE*，符合 Talos 规范
 */
class Logger {
public:
    /**
     * @brief 初始化日志系统（单例）。
     * @param log_path 日志文件路径，默认 logs/gonxun.log
     * @return 成功返回 void，失败返回错误信息
     *
     * - 清空旧日志文件
     * - 写入启动信息
     * - 重定向 stdout/stderr 到日志文件（同时保留终端输出）
     */
    [[nodiscard]] static ExpectedVoid init(const std::string& log_path = "logs/gonxun.log") noexcept;

    /// 获取单例实例
    [[nodiscard]] static Logger& instance() noexcept;

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    /// 析构时自动关闭文件
    ~Logger() noexcept;

    /// 写入日志（同时输出到终端和文件）
    void log(LogLevel level, const char* format, ...) noexcept;

private:
    Logger() = default;

    FileOwner file_;                     ///< 日志文件 RAII 持有者
    std::mutex mutex_;                   ///< 线程安全互斥锁
    int original_stdout_fd_{-1};         ///< 原始 stdout 文件描述符
    int original_stderr_fd_{-1};         ///< 原始 stderr 文件描述符
};

// ==== 便捷宏定义 ===//

/// 信息日志
#define LOG_INFO(...)  gonxun::Logger::instance().log(gonxun::LogLevel::INFO,  __VA_ARGS__)
/// 警告日志
#define LOG_WARN(...)  gonxun::Logger::instance().log(gonxun::LogLevel::WARN,  __VA_ARGS__)
/// 错误日志
#define LOG_ERROR(...) gonxun::Logger::instance().log(gonxun::LogLevel::ERROR, __VA_ARGS__)

} // namespace gonxun