/**
 * 命令行参数解析器
 * 封装 QCommandLineParser，简化参数处理
 */
#pragma once

#include <string>

class QApplication;
class QCommandLineParser;

namespace gonxun {

/**
 * 命令行参数结果
 */
struct CliOptions {
    bool mockSerial{false};       // 是否模拟串口
    bool headless{false};         // 是否无头模式
    bool simulate{false};         // 是否仿真模式
    std::string configPath;       // 配置文件路径
    std::string serialPort;       // 串口设备路径
    std::string taskCode;         // 任务码（仿真用）
};

/**
 * 命令行解析器
 * 解析启动参数并返回结果
 */
class CliParser {
public:
    /**
     * 构造函数
     * @param app Qt 应用实例
     * @param defaultConfig 默认配置文件路径
     * @param defaultSerialPort 默认串口路径
     */
    CliParser(QApplication& app,
              const std::string& defaultConfig = "config/config.yaml",
              const std::string& defaultSerialPort = "/dev/ttyCH341USB0");

    /**
     * 析构函数
     */
    ~CliParser();

    /**
     * 解析命令行参数
     * @return 解析结果
     */
    CliOptions parse();

private:
    QApplication& m_app;
    QCommandLineParser* m_parser;
    std::string m_defaultConfig;
    std::string m_defaultSerialPort;
};

} // namespace gonxun