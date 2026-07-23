/**
 * @file logger.cpp
 * @brief 通用日志系统实现。
 */

#include "logger.hpp"

#include <cstdarg>
#include <ctime>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

namespace gonxun {

namespace {

/// 创建日志目录
void create_log_directory(const std::string& filepath) {
    size_t pos = filepath.find_last_of('/');
    if (pos != std::string::npos) {
        std::string dir = filepath.substr(0, pos);
        mkdir(dir.c_str(), 0755);
    }
}

/// 获取当前时间字符串
std::string current_time_str() {
    std::time_t now = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return buf;
}

/// 日志级别转字符串
const char* level_to_str(LogLevel level) {
    switch (level) {
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
        default:              return "UNKNOWN";
    }
}

} // anonymous namespace

// ==== Logger 实现 ====

Logger& Logger::instance() noexcept {
    static Logger instance;
    return instance;
}

void Logger::init(const std::string& log_path) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (initialized_) return;

    // 创建日志目录
    create_log_directory(log_path);

    // 保存原始 stdout/stderr 的文件描述符
    original_stdout_fd_ = dup(STDOUT_FILENO);
    original_stderr_fd_ = dup(STDERR_FILENO);

    // 以截断模式打开日志文件
    file_ = std::fopen(log_path.c_str(), "w");
    if (!file_) {
        std::fprintf(stderr, "[Logger] 无法打开日志文件: %s\n", log_path.c_str());
        return;
    }

    // 获取日志文件的文件描述符
    int log_fd = fileno(file_);

    // 重定向 stdout 和 stderr 到日志文件（底层重定向，可捕获所有输出）
    dup2(log_fd, STDOUT_FILENO);
    dup2(log_fd, STDERR_FILENO);

    // 刷新 C++ 流
    std::cout.flush();
    std::cerr.flush();

    initialized_ = true;

    // 写入启动信息
    std::cout << "\n==== 程序启动 ====" << std::endl;
    std::cout << "时间: " << current_time_str() << std::endl;
    std::cout << "==================\n" << std::endl;
}

void Logger::log(LogLevel level, const char* format, ...) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!file_ || !initialized_) return;

    // 写入时间戳和级别
    std::fprintf(file_, "[%s] [%s] ", current_time_str().c_str(), level_to_str(level));

    // 写入用户消息
    va_list args;
    va_start(args, format);
    std::vfprintf(file_, format, args);
    va_end(args);

    std::fprintf(file_, "\n");
    std::fflush(file_);
}

void Logger::close() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (file_) {
        // 写入退出信息
        std::cout << "\n==== 程序退出 ====" << std::endl;
        std::cout << "时间: " << current_time_str() << std::endl;

        // 恢复原始 stdout/stderr
        if (original_stdout_fd_ >= 0) {
            dup2(original_stdout_fd_, STDOUT_FILENO);
            ::close(original_stdout_fd_);
            original_stdout_fd_ = -1;
        }
        if (original_stderr_fd_ >= 0) {
            dup2(original_stderr_fd_, STDERR_FILENO);
            ::close(original_stderr_fd_);
            original_stderr_fd_ = -1;
        }

        std::fclose(file_);
        file_ = nullptr;
    }

    initialized_ = false;
}

Logger::~Logger() {
    close();
}

} // namespace gonxun