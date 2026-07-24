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
void create_log_directory(const std::string& filepath) noexcept {
    size_t pos = filepath.find_last_of('/');
    if (pos != std::string::npos) {
        std::string dir = filepath.substr(0, pos);
        mkdir(dir.c_str(), 0755);
    }
}

/// 获取当前时间字符串
std::string current_time_str() noexcept {
    std::time_t now = std::time(nullptr);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
    return buf;
}

/// 日志级别转字符串
[[nodiscard]] const char* level_to_str(LogLevel level) noexcept {
    switch (level) {
        case LogLevel::INFO:  return "INFO";
        case LogLevel::WARN:  return "WARN";
        case LogLevel::ERROR: return "ERROR";
    }
    std::abort();  // 不可达分支
}

} // anonymous namespace

// ==== Logger 实现 ====

ExpectedVoid Logger::init(const std::string& log_path) noexcept {
    Logger& inst = instance();
    std::lock_guard<std::mutex> lock(inst.mutex_);

    if (inst.file_.valid()) {
        return {};  // 已初始化
    }

    // 创建日志目录
    create_log_directory(log_path);

    // 保存原始 stdout/stderr 的文件描述符
    inst.original_stdout_fd_ = dup(STDOUT_FILENO);
    inst.original_stderr_fd_ = dup(STDERR_FILENO);

    // 以截断模式打开日志文件
    std::FILE* file = std::fopen(log_path.c_str(), "w");
    if (!file) {
        return std::string("无法打开日志文件: " + log_path);
    }
    inst.file_ = FileOwner(file);

    // 获取日志文件的文件描述符
    int log_fd = fileno(file);

    // 重定向 stdout 和 stderr 到日志文件
    dup2(log_fd, STDOUT_FILENO);
    dup2(log_fd, STDERR_FILENO);

    // 刷新 C++ 流
    std::cout.flush();
    std::cerr.flush();

    // 写入启动信息
    std::cout << "\n==== 程序启动 ====" << std::endl;
    std::cout << "时间: " << current_time_str() << std::endl;
    std::cout << "==================\n" << std::endl;

    return {};
}

Logger& Logger::instance() noexcept {
    static Logger inst;
    return inst;
}

void Logger::log(LogLevel level, const char* format, ...) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (!file_.valid()) return;

    // 写入时间戳和级别
    std::fprintf(file_.get(), "[%s] [%s] ", current_time_str().c_str(), level_to_str(level));

    // 写入用户消息
    va_list args;
    va_start(args, format);
    std::vfprintf(file_.get(), format, args);
    va_end(args);

    std::fprintf(file_.get(), "\n");
    std::fflush(file_.get());
}

Logger::~Logger() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);

    if (file_.valid()) {
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

        file_.reset();
    }
}

} // namespace gonxun