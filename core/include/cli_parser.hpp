/// @file cli_parser.hpp
/// @brief 命令行参数解析器，封装 QCommandLineParser。
///
/// 提供统一的命令行选项定义与解析，输出 CliOptions 结构体，
/// 支持模拟串口、配置文件路径、串口设备等选项。

#pragma once

#include <string>

class QApplication;
class QCommandLineParser;

namespace gonxun {

/// @brief 命令行解析结果，包含所有可选参数。
struct CliOptions {
    bool mock_serial{false};     ///< 是否模拟串口通信（调试用）
    std::string config_path;     ///< 配置文件路径（YAML）
    std::string serial_port;     ///< 串口设备路径（如 /dev/ttyCH341USB0）
};

/// @brief 命令行参数解析器，基于 QCommandLineParser。
///
/// 支持选项：--mock-serial, --config <file>, --serial-port <port>。
class CliParser {
public:
    /// @brief 构造解析器。
    /// @param app QApplication 引用
    /// @param default_config 默认配置文件路径
    /// @param default_serial_port 默认串口设备路径
    CliParser(QApplication& app,
              const std::string& default_config = "config/config.yaml",
              const std::string& default_serial_port = "/dev/ttyCH341USB0");
    ~CliParser();

    /// @brief 解析命令行参数。
    /// @return 解析结果结构体
    [[nodiscard]] CliOptions parse();

private:
    QApplication& app_;                     ///< QApplication 引用
    QCommandLineParser* parser_;             ///< Qt 命令行解析器
    std::string default_config_;             ///< 默认配置文件路径
    std::string default_serial_port_;        ///< 默认串口设备路径
};

} // namespace gonxun