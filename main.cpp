/**
 * Gonxun 系统主入口【新版工程化版本】
 * 系统定位：智能物流视觉搬运系统，整合视觉算法、串口通信、GUI可视化界面
 * 核心特性：模块化封装、低耦合、多启动模式、优雅启停
 *
 * 完整启动用法:
 *   ./CourtMapViewer                    # 正常GUI启动，界面手动控制视觉系统启停
 *   ./CourtMapViewer --mock-serial      # 调试模式：模拟串口，无需真实硬件
 *   ./CourtMapViewer --headless         # 服务器部署：无头后台运行，无GUI界面
 *   ./CourtMapViewer --config path.yaml # 自定义加载指定配置文件
 */

// Qt基础核心依赖
#include <QApplication>     // Qt应用核心类，管理事件循环、窗口生命周期、程序全局资源
#include <QThread>          // Qt线程工具类，提供线程休眠等静态方法
#include <iostream>         // 标准控制台日志输出

// 项目自定义模块化头文件（全部抽离封装，解耦核心逻辑）
#include "mainwindow.h"     // 主窗口UI类：界面展示、用户操作、启停信号发射
#include "vision_system.hpp"// 视觉系统核心类：摄像头采集、图像预处理、算法识别、串口数据交互
#include "config_loader.hpp"// 配置文件单例加载器：解析YAML配置、全局参数统一管理
#include "app_signals.hpp"  // 应用全局信号管理：进程信号捕获、全局运行状态管控、优雅退出
#include "cli_parser.hpp"   // 命令行参数解析器：独立封装启动参数解析逻辑
#include "vision_controller.hpp" // 视觉线程控制器：统一管理视觉子线程启停、资源回收

// 程序唯一入口函数
int main(int argc, char *argv[])
{
    // ========== 1. 初始化全局信号处理（系统优雅退出） ==========
    // 封装底层信号注册逻辑：捕获Ctrl+C、系统终止信号
    // 作用：程序异常/手动终止时，安全释放摄像头、串口、线程资源，避免硬件卡死、内存泄漏
    gonxun::setupSignalHandlers();

    // ========== 2. 初始化并加载系统配置文件 ==========
    // 获取全局唯一的配置加载器实例（单例模式，全局共享一份配置）
    auto& configLoader = gonxun::ConfigLoader::instance();
    // 默认加载项目核心配置文件
    configLoader.load("config/config.yaml");
    // 获取配置只读引用，后续所有硬件参数、系统参数均从此读取
    const auto& cfg = configLoader.config();

    // ========== 3. 初始化Qt应用实例 ==========
    // 创建Qt应用核心对象，接管程序事件循环
    QApplication app(argc, argv);
    // 从配置文件读取并设置程序名称
    app.setApplicationName(QString::fromStdString(cfg.system.name));
    // 从配置文件读取并设置程序版本号
    app.setApplicationVersion(QString::fromStdString(cfg.system.version));

    // ========== 4. 解析命令行启动参数（核心解耦优化点） ==========
    // 实例化独立命令行解析器，传入：Qt应用、默认配置路径、默认串口端口
    gonxun::CliParser cliParser(app, "config/config.yaml", cfg.serial.port);
    // 执行参数解析，返回结构化的启动参数配置
    auto options = cliParser.parse();

    // 优先级覆盖：如果命令行指定了自定义配置文件路径，重新加载配置
    // 命令行参数优先级 > 默认配置文件，适配多环境部署需求
    if (options.configPath != "config/config.yaml") {
        configLoader.load(options.configPath);
    }

    // 合并参数逻辑：命令行参数 或 配置文件开启模拟串口，均启用调试模式
    bool mockSerial = options.mockSerial || cfg.serial.mock;
    // 最终生效的串口端口（优先使用命令行指定端口）
    std::string serialPort = options.serialPort;

    // ========== 5. 初始化视觉系统核心实例 ==========
    std::cout << "[Main] 初始化视觉系统..." << std::endl;
    std::cout << "[Main] 配置文件路径: " << options.configPath << std::endl;
    std::cout << "[Main] 串口设备: " << serialPort
              << " (模拟串口模式=" << (mockSerial ? "开启" : "关闭") << ")" << std::endl;

    // 构造视觉系统对象，注入所有硬件配置参数
    // 参数：模拟串口开关、串口端口、波特率、主摄像头索引、QR摄像头索引
    VisionSystem visionSystem(
        mockSerial,
        serialPort,
        cfg.serial.baudrate,
        cfg.camera.main.index,
        cfg.camera.qr.index
    );

    // ========== 6. 无头模式逻辑（后台部署模式） ==========
    // 适用于：无显示器服务器、后台常驻运行、纯算法调试场景
    if (options.headless) {
        std::cout << "[Main] 系统运行【无头后台模式】" << std::endl;
        // 全局运行状态循环，由app_signals统一管控启停
        while (gonxun::isRunning()) {
            // 读取主摄像头图像帧
            auto [success, frame] = visionSystem.camera.readMain();
            // 读取成功则执行全套视觉算法处理
            if (success) {
                visionSystem.processFrame(frame);
            }
            // 休眠1ms，释放CPU资源，避免空转满载
            QThread::msleep(1);
        }
        // 循环退出，程序结束
        return 0;
    }

    // ========== 7. 标准GUI可视化模式（人机交互模式） ==========
    std::cout << "[Main] 系统运行【GUI可视化模式】，启动界面中..." << std::endl;

    // 创建主窗口UI实例
    MainWindow window;
    // 显示主界面
    window.show();

    // 实例化视觉控制器：接管视觉线程的所有启停、资源管理逻辑
    gonxun::VisionController controller(&visionSystem);

    // Qt信号槽绑定：UI界面 与 视觉控制器解耦通信
    // 界面点击【启动视觉】按钮 -> 触发控制器启动视觉线程
    QObject::connect(&window, &MainWindow::visionStartRequested, &controller, &gonxun::VisionController::start);
    // 界面点击【停止视觉】按钮 -> 触发控制器停止视觉线程
    QObject::connect(&window, &MainWindow::visionStopRequested, &controller, &gonxun::VisionController::stop);

    // ========== 8. 启动Qt事件循环（程序主循环） ==========
    // 阻塞执行，监听界面点击、信号触发、窗口事件
    int ret = app.exec();

    // ========== 9. 程序退出收尾：安全释放资源 ==========
    // 主动停止视觉子线程，回收摄像头、算法资源
    controller.stop();
    // 标记全局程序退出状态，终止所有后台循环
    gonxun::requestExit();

    std::cout << "[Main] 系统安全退出，资源已全部释放" << std::endl;
    return ret;
}
