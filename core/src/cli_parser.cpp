/**
 * @file cli_parser.cpp
 * @brief 命令行参数解析器实现文件
 * 
 * @details 本文件实现了基于 Qt 的命令行参数解析功能。
 *          核心特性：
 *          - Qt 集成：使用 QCommandLineParser 解析参数
 *          - 多种模式：支持调试、仿真、无头等多种运行模式
 *          - 默认值：为所有参数提供合理的默认值
 *          - 类型安全：将参数封装为 CliOptions 结构体
 * 
 * @author 智能物流搬运系统开发团队
 * @version 1.0
 * @date 2025-01-01
 * 
 * @note 修改历史：
 *       - 2025-01-01: 初始版本，支持基础参数
 *       - 2025-02-15: 增加仿真模式参数
 *       
 * @note 支持的命令行参数：
 *       - --mock-serial: 模拟串口通信（调试用）
 *       - --headless: 无头模式（不显示 GUI）
 *       - --simulate: 仿真模式（不连接硬件，模拟任务流程）
 *       - --config <file>: 配置文件路径（默认：config/default.json）
 *       - --serial-port <port>: 串口设备路径（默认：/dev/ttyUSB0）
 *       - --task-code <code>: 任务码（3位数字，如 123/312/213）
 *       
 * @note 使用示例：
 *       ```bash
 *       # 调试模式（模拟串口）
 *       ./gonxun --mock-serial --task-code 123
 *       
 *       # 仿真模式（不连接硬件）
 *       ./gonxun --simulate --headless
 *       
 *       # 指定配置文件和串口
 *       ./gonxun --config myconfig.json --serial-port /dev/ttyACM0
 *       ```
 *       
 * @see cli_parser.hpp
 */
#include "cli_parser.hpp"
#include <QApplication>
#include <QCommandLineParser>

namespace gonxun {

/**
 * @brief 构造函数，初始化解析器
 * 
 * @details 创建 QCommandLineParser 对象，并设置默认值。
 *          
 * @param app Qt 应用程序对象（用于获取应用程序信息）
 * @param defaultConfig 默认配置文件路径（如 "config/default.json"）
 * @param defaultSerialPort 默认串口设备路径（如 "/dev/ttyUSB0"）
 * 
 * @note 注意事项：
 *       - 必须在 QApplication 创建后才能构造 CliParser
 *       - m_parser 使用 new 分配，析构时需要手动 delete
 */
CliParser::CliParser(QApplication& app,
                     const std::string& defaultConfig,
                     const std::string& defaultSerialPort)
    : m_app(app)
    , m_parser(new QCommandLineParser)
    , m_defaultConfig(defaultConfig)
    , m_defaultSerialPort(defaultSerialPort) {}

/**
 * @brief 析构函数，释放解析器对象
 */
CliParser::~CliParser() {
    delete m_parser;  // 释放 QCommandLineParser 对象
}

/**
 * @brief 解析命令行参数
 * 
 * @details 定义所有支持的参数选项，解析命令行，并返回结构化的选项对象。
 *          
 * @return CliOptions 解析后的选项结构体
 *         - mockSerial: 是否启用串口模拟模式
 *         - headless: 是否启用无头模式（不显示 GUI）
 *         - simulate: 是否启用仿真模式（不连接硬件）
 *         - configPath: 配置文件路径（默认：m_defaultConfig）
 *         - serialPort: 串口设备路径（默认：m_defaultSerialPort）
 *         - taskCode: 任务码（默认："123"）
 *         
 * @note 参数定义：
 *       - "mock-serial": 无参数标志，用于调试
 *       - "headless": 无参数标志，用于服务器环境
 *       - "simulate": 无参数标志，用于测试
 *       - "config": 带参数选项，需要文件路径
 *       - "serial-port": 带参数选项，需要设备路径
 *       - "task-code": 带参数选项，需要3位数字
 *       
 * @note 使用流程：
 *       1. 设置应用程序描述
 *       2. 添加帮助和版本选项（自动处理 --help 和 --version）
 *       3. 定义所有参数选项
 *       4. 调用 process() 解析参数
 *       5. 提取参数值到 CliOptions 结构体
 *       
 * @see CliOptions
 */
CliOptions CliParser::parse() {
    // 设置应用程序描述（用于 --help 输出）
    m_parser->setApplicationDescription("工创赛2025智能物流搬运系统");
    m_parser->addHelpOption();      // 自动添加 --help 选项
    m_parser->addVersionOption();   // 自动添加 --version 选项

    // 定义参数选项
    QCommandLineOption mockSerialOption("mock-serial", "模拟串口通信（调试用）");
    QCommandLineOption headlessOption("headless", "无头模式（不显示 GUI）");
    QCommandLineOption simulateOption("simulate", "仿真模式（不连接硬件，模拟任务流程）");
    
    // 带参数的选项（参数名，描述，参数占位符，默认值）
    QCommandLineOption configOption("config", "配置文件路径", "file", m_defaultConfig.c_str());
    QCommandLineOption serialPortOption("serial-port", "串口设备路径", "port", m_defaultSerialPort.c_str());
    QCommandLineOption taskCodeOption("task-code", "任务码（3位数字，如 123/312/213）", "code", "123");

    // 注册所有选项到解析器
    m_parser->addOption(mockSerialOption);
    m_parser->addOption(headlessOption);
    m_parser->addOption(simulateOption);
    m_parser->addOption(configOption);
    m_parser->addOption(serialPortOption);
    m_parser->addOption(taskCodeOption);

    // 解析命令行参数（会自动处理 --help 和 --version）
    m_parser->process(m_app);

    // 提取参数值到结构体
    CliOptions options;
    options.mockSerial = m_parser->isSet(mockSerialOption);              // 检查标志是否存在
    options.headless = m_parser->isSet(headlessOption);                  // 检查标志是否存在
    options.simulate = m_parser->isSet(simulateOption);                  // 检查标志是否存在
    options.configPath = m_parser->value(configOption).toStdString();    // 获取参数值
    options.serialPort = m_parser->value(serialPortOption).toStdString(); // 获取参数值
    options.taskCode = m_parser->value(taskCodeOption).toStdString();    // 获取参数值

    return options;
}

} // namespace gonxun