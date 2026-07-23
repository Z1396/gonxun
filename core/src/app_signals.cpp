/**
 * @file app_signals.cpp
 * @brief 应用程序信号处理实现。
 *
 * 定义全局原子运行标志 g_running，注册 SIGINT/SIGTERM 处理器
 * 实现优雅退出，提供运行状态查询和主动退出请求接口。
 * 
 * @details 核心设计：
 * 1. 原子变量保证多线程下运行状态读写安全
 * 2. 捕获系统标准退出信号，实现外部触发优雅退出
 * 3. 联动Qt事件循环，适配Qt程序退出逻辑
 * 4. 提供对外通用接口，支持代码主动退出、状态查询
 * @author gonxun
 * @date 2026
 */

// 引入对应头文件（声明、系统库、Qt核心库）
#include "app_signals.hpp"   // 本模块头文件，对外函数、变量声明

#include <csignal>          // C++标准信号库：提供signal()信号注册函数、SIGINT/SIGTERM信号定义
#include <iostream>         // 标准输出库：用于打印退出日志信息
#include <QCoreApplication> // Qt核心应用类：控制Qt事件循环启停

// 项目命名空间，隔离全局作用域，避免命名冲突
namespace gonxun {

/**
 * @brief 全局原子运行状态标志
 * @note std::atomic 原子变量：保证多线程环境下读写无数据竞争，线程安全
 * @value true  - 程序正常运行中
 * @value false - 程序需要退出/正在退出
 * @design 全局变量，整个项目任意位置可查询程序运行状态
 */
std::atomic<bool> g_running{true};

// 匿名命名空间：仅当前文件内有效，封装私有信号处理函数，对外隐藏实现细节
namespace {

/**
 * @brief 系统信号统一处理回调函数
 * @param signal 触发的系统信号编号
 * @note 该函数为信号中断上下文执行，逻辑必须极简、安全
 * @function 1. 打印退出日志，提示用户程序收到退出信号
 *          2. 将全局运行标志置为false，通知所有业务线程停止循环
 *          3. 退出Qt核心事件循环，终止Qt程序生命周期
 * @signal 触发场景：
 * SIGINT  : 用户控制台按下 Ctrl+C 触发
 * SIGTERM : 系统/进程管理器主动终止程序（docker stop、kill 进程默认信号）
 */
void signal_handler(int signal) {
    // 打印换行+日志，避免控制台光标粘连，提示退出原因
    std::cout << "\n收到信号 " << signal << "，正在退出..." << std::endl;
    
    // 标记程序退出状态：原子赋值，多线程实时可见
    g_running = false;

    // 退出Qt事件循环，等效 QCoreApplication::exit(0)
    // Qt程序核心：所有Qt定时器、信号槽、事件分发均依赖事件循环
    // 调用quit()后，事件循环结束，main函数QCoreApplication::exec()返回
    QCoreApplication::quit();
}

} // anonymous namespace

/**
 * @brief 注册系统信号处理器（对外初始化接口）
 * @function 绑定 SIGINT、SIGTERM 两个核心退出信号到自定义处理函数
 * @usage 程序启动初期（main函数初始化Qt后）调用一次
 * @note 覆盖系统默认信号行为：
 * 默认SIGINT/SIGTERM会直接强制终止程序，无清理流程
 * 自定义处理器后，程序会执行优雅退出逻辑
 */
void setup_signal_handlers() {
    // 注册Ctrl+C中断信号处理器
    std::signal(SIGINT, signal_handler);
    // 注册系统终止信号处理器
    std::signal(SIGTERM, signal_handler);
}

/**
 * @brief 查询程序当前运行状态（线程安全）
 * @return true 程序正常运行，可执行业务逻辑
 * @return false 程序待退出，需停止业务、释放资源
 * @design 业务线程循环核心判断条件
 * @example while (is_running()) { 执行业务逻辑; }
 */
bool is_running() {
    // load()：原子读取变量值，保证多线程下获取最新状态
    return g_running.load();
}

/**
 * @brief 主动请求程序退出（代码触发优雅退出）
 * @function 纯状态标记，配合Qt事件循环可扩展主动退出逻辑
 * @usage 业务代码中需要主动退出程序时调用
 * @note 仅修改状态，如需完整退出，可后续调用QCoreApplication::quit()
 */
void request_exit() {
    // store()：原子写入变量值，所有线程实时感知退出状态
    g_running.store(false);
}

} // namespace gonxun