/**
 * @file common_types.hpp
 * @brief 项目通用数据类型定义。
 *
 * 定义点坐标、路径、矩形障碍物等通用数据结构，
 * 供路径规划、任务状态机、视觉系统等模块统一引用。
 */

#pragma once

#include "field_constants.hpp"

#include <cstdio>
#include <unistd.h>
#include <cstddef>
#include <variant>
#include <vector>
#include <string>
#include <utility>
#include <functional>

namespace gonxun {

// ==== overloaded 辅助工具（用于 std::visit） ====

/**
 * @brief 重载集辅助工具，简化 std::visit 的 lambda 写法。
 *
 * 用法：std::visit(overloaded{[](Type1&){...}, [](Type2&){...}}, variant);
 */
template<class... Ts>
struct overloaded : Ts... {
    using Ts::operator()...;
};
template<class... Ts>
overloaded(Ts...) -> overloaded<Ts...>;

// ==== C++17 兼容的 expected 实现 ====

/**
 * @brief 简化的 expected 类型，用于表达成功或失败。
 *
 * 由于 C++17 没有 std::expected，使用 std::variant 模拟。
 * 成功时持有 T，失败时持有 std::string 错误信息。
 */
template<typename T>
using Expected = std::variant<T, std::string>;

/// 特化：void 类型使用 bool 表示成功
using ExpectedVoid = std::variant<std::monostate, std::string>;

template<typename T>
[[nodiscard]] bool is_success(const Expected<T>& exp) noexcept {
    return std::holds_alternative<T>(exp);
}

[[nodiscard]] inline bool is_success(const ExpectedVoid& exp) noexcept {
    return std::holds_alternative<std::monostate>(exp);
}

template<typename T>
[[nodiscard]] bool is_error(const Expected<T>& exp) noexcept {
    return std::holds_alternative<std::string>(exp);
}

[[nodiscard]] inline bool is_error(const ExpectedVoid& exp) noexcept {
    return std::holds_alternative<std::string>(exp);
}

template<typename T>
[[nodiscard]] T& get_value(Expected<T>& exp) noexcept {
    return std::get<T>(exp);
}

template<typename T>
[[nodiscard]] const T& get_value(const Expected<T>& exp) noexcept {
    return std::get<T>(exp);
}

template<typename T>
[[nodiscard]] std::string& get_error(Expected<T>& exp) noexcept {
    return std::get<std::string>(exp);
}

template<typename T>
[[nodiscard]] const std::string& get_error(const Expected<T>& exp) noexcept {
    return std::get<std::string>(exp);
}

[[nodiscard]] inline std::string& get_error(ExpectedVoid& exp) noexcept {
    return std::get<std::string>(exp);
}

[[nodiscard]] inline const std::string& get_error(const ExpectedVoid& exp) noexcept {
    return std::get<std::string>(exp);
}

// ==== RAII 资源管理类型 ====

/**
 * @brief RAII 文件描述符包装器。
 *
 * 管理裸文件描述符（int fd），确保在析构时自动关闭。
 * 禁止拷贝，允许移动，符合 Talos 规范的资源管理要求。
 */
class FileDescriptor {
public:
    explicit FileDescriptor(int fd = -1) noexcept : fd_(fd) {}
    FileDescriptor(FileDescriptor&& other) noexcept : fd_(other.fd_) {
        other.fd_ = -1;
    }
    FileDescriptor& operator=(FileDescriptor&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) {
                ::close(fd_);
            }
            fd_ = other.fd_;
            other.fd_ = -1;
        }
        return *this;
    }
    FileDescriptor(const FileDescriptor&) = delete;
    FileDescriptor& operator=(const FileDescriptor&) = delete;
    ~FileDescriptor() noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    [[nodiscard]] int get() const noexcept { return fd_; }
    [[nodiscard]] bool valid() const noexcept { return fd_ >= 0; }
    void reset(int fd = -1) noexcept {
        if (fd_ >= 0) {
            ::close(fd_);
        }
        fd_ = fd;
    }
private:
    int fd_;
};

/**
 * @brief RAII FILE* 包装器。
 *
 * 管理 C 标准库 FILE* 指针，确保在析构时自动关闭。
 * 禁止拷贝，允许移动，符合 Talos 规范的资源管理要求。
 */
class FileOwner {
public:
    explicit FileOwner(std::FILE* file = nullptr) noexcept : file_(file) {}
    FileOwner(FileOwner&& other) noexcept : file_(other.file_) {
        other.file_ = nullptr;
    }
    FileOwner& operator=(FileOwner&& other) noexcept {
        if (this != &other) {
            if (file_) {
                std::fclose(file_);
            }
            file_ = other.file_;
            other.file_ = nullptr;
        }
        return *this;
    }
    FileOwner(const FileOwner&) = delete;
    FileOwner& operator=(const FileOwner&) = delete;
    ~FileOwner() noexcept {
        if (file_) {
            std::fclose(file_);
        }
    }
    [[nodiscard]] std::FILE* get() const noexcept { return file_; }
    [[nodiscard]] bool valid() const noexcept { return file_ != nullptr; }
    void reset(std::FILE* file = nullptr) noexcept {
        if (file_) {
            std::fclose(file_);
        }
        file_ = file;
    }
private:
    std::FILE* file_;
};

/// 栅格分辨率，每格代表 50mm
constexpr int GRID_RESOLUTION_MM = 50;
/// 栅格边长 = FIELD_SIZE_MM / GRID_RESOLUTION_MM = 48
constexpr int GRID_SIZE = FIELD_SIZE_MM / GRID_RESOLUTION_MM;

// ==== 强类型包装（避免 primitive obsession） ====

/**
 * @brief 毫米单位强类型包装。
 */
struct Millimeters {
    int value{0};
    [[nodiscard]] bool operator==(const Millimeters& o) const { return value == o.value; }
    [[nodiscard]] bool operator!=(const Millimeters& o) const { return value != o.value; }
};

/**
 * @brief 栅格索引强类型包装。
 */
struct GridIndex {
    int value{0};
    [[nodiscard]] bool operator==(const GridIndex& o) const { return value == o.value; }
    [[nodiscard]] bool operator!=(const GridIndex& o) const { return value != o.value; }
};

// ==== 数据结构 ====

/**
 * @brief 二维点坐标，单位取决于上下文（mm 或栅格索引）。
 */
struct Point {
    int x{0};  ///< X 坐标
    int y{0};  ///< Y 坐标

    bool operator==(const Point& o) const { return x == o.x && y == o.y; }
    bool operator!=(const Point& o) const { return x != o.x || y != o.y; }
};

/**
 * @brief Point 的哈希函数，用于 unordered_map/unordered_set。
 */
struct PointHash {
    [[nodiscard]] size_t operator()(const Point& p) const noexcept {
        return static_cast<size_t>(p.x) * 1000 + p.y;
    }
};

/// 路径类型，由一系列 Point 组成
using Path = std::vector<Point>;

/**
 * @brief 轴对齐矩形障碍物，坐标和尺寸单位为 mm。
 */
struct ObstacleRect {
    int x;  ///< 左上角 X
    int y;  ///< 左上角 Y
    int w;  ///< 宽度
    int h;  ///< 高度
};

} // namespace gonxun
