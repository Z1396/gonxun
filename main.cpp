/// @file main.cpp
/// @brief Gonxun 智能物流搬运系统主入口（GUI模式）
///
/// 【系统完整启动流水线（固定顺序）】
///  1. 注册全局信号钩子：支持终端 Ctrl+C、系统 kill 优雅退出
///  2. 加载 YAML 全局系统配置（参数统一中心化管理）
///  3. 初始化 Qt 应用核心实例
///  4. 解析命令行参数，支持覆盖配置文件、模拟串口、自定义设备
///  5. 实例化顶层视觉业务系统（相机、YOLO、卡尔曼、串口、二维码、圆环检测）
///  6. 创建UI主窗口、视觉线程控制器
///  7. 绑定UI按钮信号 ↔ 视觉线程启停槽函数
///  8. 启动Qt事件循环，常驻运行
///  9. 程序退出时自动停止线程、释放资源、安全收尾
///
/// 【模块依赖关系】
///  MainWindow(UI) --控制--> VisionController(线程管理器)
///  VisionController --调度--> VisionSystem(视觉算法业务)
///  VisionSystem --依赖--> ConfigLoader(全局配置)

#include "mainwindow.hpp"        // UI主窗口
#include "vision_system.hpp"     // 视觉算法总系统
#include "config_loader.hpp"     // YAML配置加载器
#include "app_signals.hpp"       // 系统信号优雅退出
#include "cli_parser.hpp"        // 命令行参数解析
#include "vision_controller.hpp" // 视觉线程控制器

#include <QApplication>
#include <iostream>

/**
 * @brief 程序唯一入口函数
 * @param argc 参数个数
 * @param argv 参数数组
 * @return int 程序退出码
 * @note 所有模块生命周期全部由 main 统一管理
 */
int main(int argc, char* argv[])
{
    // ===================== 1. 注册全局系统信号处理器 =====================
    // 作用：捕获 SIGINT(Ctrl+C) / SIGTERM(程序终止)
    // 实现后台线程、串口、资源优雅释放，杜绝僵尸进程、串口卡死
    gonxun::setup_signal_handlers();

    // ===================== 2. 加载全局YAML配置 =====================
    // 单例配置加载器，全局唯一
    auto& config_loader = gonxun::ConfigLoader::instance();
    // 加载默认配置文件 config/config.yaml
    if (!config_loader.load("config/config.yaml")) {
        std::cerr << "[Main] 警告: 加载默认配置文件失败，使用内置默认值" << std::endl;
    }
    // 获取全局配置引用（所有模块参数来源）
    const auto& cfg = config_loader.config();

    // ===================== 3. 初始化Qt应用实例 =====================
    // Qt程序必须最先初始化QApplication
    QApplication app(argc, argv);
    // 设置应用名称、版本（用于窗口信息、日志、打包信息）
    app.setApplicationName(QString::fromStdString(cfg.system.name));
    app.setApplicationVersion(QString::fromStdString(cfg.system.version));

    // ===================== 4. 解析命令行启动参数 =====================
    // 支持启动时自定义：配置文件路径、串口设备、模拟模式
    gonxun::CliParser cli_parser(app, "config/config.yaml", cfg.serial.port);
    auto options = cli_parser.parse();

    // 如果命令行指定了自定义配置文件，则重新加载覆盖默认配置
    if (options.config_path != "config/config.yaml") {
        if (!config_loader.load(options.config_path)) {
            std::cerr << "[Main] 警告: 加载配置文件失败: " << options.config_path << std::endl;
        }
    }

    // 串口模拟模式优先级：命令行参数 > 配置文件
    bool mock_serial = options.mock_serial || cfg.serial.mock;
    // 串口设备名优先使用命令行传入
    std::string serial_port = options.serial_port;

    // ===================== 5. 初始化【视觉总系统】 =====================
    // 包含：双相机、YOLO检测、卡尔曼滤波、圆环检测、二维码检测、串口通信
    // 所有底层硬件+算法模块在此统一构造
    std::cout << "[Main] 初始化视觉系统..." << std::endl;
    std::cout << "[Main] 配置文件路径: " << options.config_path << std::endl;
    std::cout << "[Main] 串口设备: " << serial_port
              << " (模拟串口=" << (mock_serial ? "开启" : "关闭") << ")" << std::endl;

    VisionSystem vision_system(
        mock_serial,                     // 是否模拟串口
        serial_port,                     // 串口设备名
        cfg.serial.baudrate,             // 波特率
        cfg.camera.main.index,           // 主相机ID
        cfg.camera.qr.index              // 扫码相机ID
    );

    // ===================== 6. 启动GUI主窗口 =====================
    std::cout << "[Main] 启动 GUI 界面..." << std::endl;

    // 创建主窗口（UI布局、按钮、地图、数据面板全部初始化）
    MainWindow window;
    window.show();

    // 创建【视觉线程控制器】（管理子线程启停）
    // 绑定上层视觉算法系统，实现线程解耦
    gonxun::VisionController controller(&vision_system);

    // ===================== 7. 核心信号槽绑定：UI ↔ 视觉线程 =====================
    /**
     * 链路：用户点击UI按钮 ==> 发射信号 ==> 控制后台算法线程
     * 完全解耦：UI不知道线程细节，线程不知道UI细节
     */
    // UI点击【开始】 --> 启动视觉子线程（相机采集+算法循环）
    QObject::connect(&window, &MainWindow::vision_start_requested,
                     &controller, &gonxun::VisionController::start);
    // UI点击【停止】 --> 终止视觉子线程、安全回收资源
    QObject::connect(&window, &MainWindow::vision_stop_requested,
                     &controller, &gonxun::VisionController::stop);

    // ===================== 8. Qt主线程事件循环（程序常驻） =====================
    // 阻塞式运行，监听UI点击、信号、刷新、所有事件
    int ret = app.exec();

    // ===================== 9. 程序退出安全收尾 =====================
    // 主动停止视觉线程，防止子线程残留、内存泄漏、相机占用
    controller.stop();
    // 通知全局系统退出，关闭串口、相机、释放资源
    gonxun::request_exit();

    std::cout << "[Main] 系统安全退出" << std::endl;
    return ret;
}
