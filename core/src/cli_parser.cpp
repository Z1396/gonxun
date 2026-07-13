/**
 * 命令行参数解析器实现
 */

#include "cli_parser.hpp"
#include <QApplication>
#include <QCommandLineParser>

namespace gonxun {

CliParser::CliParser(QApplication& app,
                     const std::string& defaultConfig,
                     const std::string& defaultSerialPort)
    : m_app(app)
    , m_parser(new QCommandLineParser)
    , m_defaultConfig(defaultConfig)
    , m_defaultSerialPort(defaultSerialPort) {}

CliParser::~CliParser() {
    delete m_parser;
}

CliOptions CliParser::parse() {
    m_parser->setApplicationDescription("工创赛2025智能物流搬运系统");
    m_parser->addHelpOption();
    m_parser->addVersionOption();

    QCommandLineOption mockSerialOption("mock-serial", "模拟串口通信（调试用）");
    QCommandLineOption headlessOption("headless", "无头模式（不显示 GUI）");
    QCommandLineOption configOption("config", "配置文件路径", "file", m_defaultConfig.c_str());
    QCommandLineOption serialPortOption("serial-port", "串口设备路径", "port", m_defaultSerialPort.c_str());

    m_parser->addOption(mockSerialOption);
    m_parser->addOption(headlessOption);
    m_parser->addOption(configOption);
    m_parser->addOption(serialPortOption);

    m_parser->process(m_app);

    CliOptions options;
    options.mockSerial = m_parser->isSet(mockSerialOption);
    options.headless = m_parser->isSet(headlessOption);
    options.configPath = m_parser->value(configOption).toStdString();
    options.serialPort = m_parser->value(serialPortOption).toStdString();

    return options;
}

} // namespace gonxun