/// @file cli_parser.cpp
/// @brief 命令行参数解析器实现。
///
/// 基于 Qt 原生 QCommandLineParser 实现命令行参数解析。
/// 支持参数：
///   --mock-serial      模拟串口开关（布尔开关，无参数）
///   --config file      自定义YAML配置文件路径
///   --serial-port port 自定义串口设备路径

#include "cli_parser.hpp"

#include <QApplication>
#include <QCommandLineParser>

namespace gonxun {

CliParser::CliParser(QApplication& app,
                     const std::string& default_config,
                     const std::string& default_serial_port)
    : app_(app)
    , parser_(new QCommandLineParser)
    , default_config_(default_config)
    , default_serial_port_(default_serial_port) {}

CliParser::~CliParser() {
    delete parser_;
}

CliOptions CliParser::parse() {
    parser_->setApplicationDescription("工创赛2025智能物流搬运系统");
    parser_->addHelpOption();
    parser_->addVersionOption();

    // 开关型参数
    QCommandLineOption mock_serial_option("mock-serial", "模拟串口通信（调试用）");

    // 传值型参数
    QCommandLineOption config_option("config", "配置文件路径", "file", default_config_.c_str());
    QCommandLineOption serial_port_option("serial-port", "串口设备路径", "port", default_serial_port_.c_str());

    parser_->addOption(mock_serial_option);
    parser_->addOption(config_option);
    parser_->addOption(serial_port_option);

    parser_->process(app_);

    CliOptions options;
    options.mock_serial = parser_->isSet(mock_serial_option);
    options.config_path = parser_->value(config_option).toStdString();
    options.serial_port = parser_->value(serial_port_option).toStdString();

    return options;
}

} // namespace gonxun