/// @file main.cpp
/// @brief Gonxun 智能物流搬运系统主入口（GUI模式）
///
/// 【系统完整启动流水线（固定顺序）】
///  1. 解析命令行参数（配置路径、调试开关）
///  2. 注册全局信号钩子 + 初始化日志
///  3. 加载 YAML 配置（命令行可覆盖）
///  4. 初始化 Qt 应用核心实例
///  5. 实例化顶层视觉业务系统
///  6. 创建 UI 主窗口 + 信号槽绑定
///  7. 启动 Qt 事件循环
///  8. 程序退出安全收尾
///
/// 【模块依赖关系】
///  MainWindow(UI) --控制--> VisionController(线程管理器)
///  VisionController --调度--> VisionSystem(视觉算法业务)
///  VisionSystem --依赖--> ConfigLoader(全局配置)

#include "mainwindow.hpp"
#include "vision_system.hpp"
#include "config_loader.hpp"
#include "app_signals.hpp"
#include "vision_controller.hpp"
#include "logger.hpp"

#include <opencv2/core.hpp>
#include <QApplication>
#include <iostream>

const std::string cli_keys =
  "{help h usage ? |      | 显示命令行参数说明}"
  "{@config-path   | config/config.yaml | YAML 配置文件路径}"
  "{mock-serial    |      | 使用模拟串口（调试模式）}"
  "{serial-port    |      | 覆盖串口设备路径（如 /dev/ttyUSB0）}";

int main(int argc, char* argv[])
{
    // ===================== 1. 解析命令行参数 =====================
    cv::CommandLineParser cli(argc, argv, cli_keys);
    if (cli.has("help")) {
        cli.printMessage();
        return 0;
    }
    auto config_path = cli.get<std::string>(0);

    // ===================== 2. 注册全局系统信号处理器 =====================
    gonxun::setup_signal_handlers();

    // ===================== 2.5 初始化日志系统 =====================
    auto log_exp = gonxun::Logger::init("logs/gonxun.log");
    if (gonxun::is_error(log_exp)) {
        std::cerr << "[Main] 日志初始化失败: " << gonxun::get_error(log_exp) << std::endl;
        return 1;
    }
    LOG_INFO("程序启动中... 配置文件: {}", config_path.c_str());

    // ===================== 3. 加载全局YAML配置 =====================
    auto config_exp = gonxun::ConfigLoader::init(config_path);
    if (gonxun::is_error(config_exp)) {
        std::cerr << "[Main] 配置加载失败: " << gonxun::get_error(config_exp) << std::endl;
        return 1;
    }
    auto& cfg = gonxun::ConfigLoader::instance().config();

    // 命令行参数覆盖 YAML 配置
    if (cli.has("mock-serial"))  cfg.serial.mock = true;
    if (cli.has("serial-port"))  cfg.serial.port = cli.get<std::string>("serial-port");

    // ===================== 4. 初始化Qt应用实例 =====================
    QApplication app(argc, argv);
    app.setApplicationName(QString::fromStdString(cfg.system.name));
    app.setApplicationVersion(QString::fromStdString(cfg.system.version));

    // ===================== 5. 构造唯一串口实例（注入共享） =====================
    std::cout << "[Main] 初始化视觉系统..." << std::endl;
    std::cout << "[Main] 串口设备: " << cfg.serial.port
              << " (模拟串口=" << (cfg.serial.mock ? "开启" : "关闭") << ")" << std::endl;

    auto serial_exp = gonxun::SerialComm::create(cfg.serial.mock, cfg.serial.port, cfg.serial.baudrate);
    if (gonxun::is_error(serial_exp)) {
        std::cerr << "[Main] 串口初始化失败: " << gonxun::get_error(serial_exp) << std::endl;
        return 1;
    }
    auto serial_comm_ptr = std::move(gonxun::get_value(serial_exp));
    gonxun::SerialComm& serial_comm = *serial_comm_ptr;

    // ===================== 6. 视觉系统 + GUI 主窗口（注入 serial_comm） =====================
    VisionSystem vision_system(cfg, serial_comm);

    std::cout << "[Main] 启动 GUI 界面..." << std::endl;
    MainWindow window(serial_comm, vision_system);
    window.show();

    // 注册二维码扫描回调
    vision_system.set_qr_callback([&window](const std::string& qr_data) {
        QString task_code = QString::fromStdString(qr_data);
        window.qr_code_scanned(task_code);
    });

    gonxun::VisionController controller(&vision_system);

    // ---- UI ↔ 视觉线程信号槽绑定 ----
    QObject::connect(&window, &MainWindow::vision_start_requested,
                     &controller, &gonxun::VisionController::start);
    QObject::connect(&window, &MainWindow::vision_stop_requested,
                     &controller, &gonxun::VisionController::stop);

    QObject::connect(&controller, &gonxun::VisionController::frame_ready,
                     &window, &MainWindow::on_frame_ready);
    QObject::connect(&controller, &gonxun::VisionController::qr_frame_ready,
                     &window, &MainWindow::on_qr_frame_ready);

    QObject::connect(&window, &MainWindow::mode_switch_requested,
        [&vision_system](int mode, bool manual) {
            vision_system.manual_mode_.store(manual, std::memory_order_relaxed);
            vision_system.override_unit_.store(static_cast<uint8_t>(mode), std::memory_order_relaxed);
        });

    // ===================== 7. 启动串口接收线程（回调已全部注册） =====================
    serial_comm.start();

    // ===================== 8. Qt主线程事件循环 =====================
    int ret = app.exec();

    // ===================== 9. 程序退出安全收尾 =====================
    controller.stop();
    gonxun::request_exit();

    LOG_INFO("系统安全退出");

    return ret;
}