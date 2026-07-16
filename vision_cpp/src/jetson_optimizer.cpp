/**
 * @file jetson_optimizer.cpp
 * @brief Jetson Nano 性能优化模块实现文件
 * 
 * @details 本文件实现了针对 NVIDIA Jetson Nano 嵌入式平台的性能优化功能。
 *          核心功能：
 *          - 平台检测：自动识别是否运行在 Jetson 平台
 *          - 内存监控：实时监控 GPU 内存使用情况
 *          - GPU 频率调节：读取当前 GPU 运行频率
 *          - 性能模式控制：开启/关闭最大性能模式（jetson_clocks）
 *          - 参数优化：根据内存使用情况自动调整推理参数
 *          - 性能监控线程：后台持续监控系统状态
 * 
 * @author 智能物流搬运系统开发团队
 * @version 1.0
 * @date 2025-01-01
 * 
 * @note 修改历史：
 *       - 2025-01-01: 初始版本，移植自 Python 版本 vision/jetson_optimizer.py
 *       - 2025-02-10: 增加 JetsonPerformanceMonitor 类，支持后台线程监控
 *       - 2025-03-15: 优化内存解析逻辑，提高稳定性
 * 
 * @note 平台依赖：
 *       - 依赖 Jetson 特有的系统文件：/etc/nv_tegra_release
 *       - 依赖 tegrastats 工具获取内存信息
 *       - 依赖 jetson_clocks 工具调节性能模式
 *       
 * @see jetson_optimizer.hpp
 */
#include "jetson_optimizer.hpp"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <chrono>

/**
 * @brief 检测当前平台是否为 Jetson 系列
 * 
 * @details 通过检查 /etc/nv_tegra_release 文件是否存在来判断。
 *          该文件是 Jetson 平台的标识文件。
 * 
 * @return bool 
 *         - true: 运行在 Jetson 平台（Jetson Nano/TX2/Xavier 等）
 *         - false: 非 Jetson 平台（普通 PC、服务器等）
 * 
 * @note 使用场景：
 *       在调用其他 Jetson 特定功能前，应先调用此函数验证平台，
 *       避免在非 Jetson 平台上执行无效操作。
 */
bool isJetsonPlatform() {
    std::ifstream f("/etc/nv_tegra_release");
    return f.good();  // 文件存在则为 Jetson 平台
}

/**
 * @brief 获取 GPU 内存使用信息
 * 
 * @details 使用 tegrastats 工具获取 Jetson 平台的内存使用情况。
 *          解析 tegrastats 输出的第一行，提取已用内存和总内存。
 * 
 * @return MemoryInfo 内存信息结构体
 *         - usedMb: 已用内存（MB）
 *         - totalMb: 总内存（MB）
 *         - freeMb: 空闲内存（MB）
 *         - usagePercent: 内存使用率（百分比）
 * 
 * @note tegrastats 输出格式：
 *       示例: "RAM 2048/3964MB (lfb 2x4MB) CPU ..."
 *       解析逻辑：
 *       1. 查找 "RAM " 字符串位置
 *       2. 找到 "MB" 标识符
 *       3. 提取 "RAM " 和 "MB" 之间的字符串（如 "2048/3964"）
 *       4. 按照斜杠分割，得到已用和总内存
 *       
 * @note 注意事项：
 *       - 仅在 Jetson 平台上有效，其他平台返回空结构体
 *       - 使用 popen 执行 shell 命令，有一定性能开销
 *       - 需要确保 tegrastats 工具可用
 *       
 * @see isJetsonPlatform()
 */
MemoryInfo getGpuMemoryInfo() {
    MemoryInfo info{0, 0, 0, 0.0};
    
    // 非 Jetson 平台直接返回空结构体
    if (!isJetsonPlatform()) return info;

    // 使用 tegrastats 获取内存信息
    FILE* pipe = popen("tegrastats --interval 1", "r");
    if (!pipe) return info;

    // 读取 tegrastats 输出的第一行
    char buffer[512];
    if (fgets(buffer, sizeof(buffer), pipe)) {
        std::string output(buffer);
        
        // 解析内存信息
        // 示例输出: "RAM 2048/3964MB (lfb 2x4MB) CPU ..."
        auto ramPos = output.find("RAM ");
        if (ramPos != std::string::npos) {
            auto mbPos = output.find("MB", ramPos);
            if (mbPos != std::string::npos) {
                // 提取 "RAM " 和 "MB" 之间的字符串（如 "2048/3964"）
                std::string parts = output.substr(ramPos + 4, mbPos - ramPos - 4);
                auto slashPos = parts.find('/');
                
                if (slashPos != std::string::npos) {
                    // 解析已用内存（斜杠前）
                    info.usedMb = std::atoi(parts.substr(0, slashPos).c_str());
                    // 解析总内存（斜杠后）
                    info.totalMb = std::atoi(parts.substr(slashPos + 1).c_str());
                    // 计算空闲内存
                    info.freeMb = info.totalMb - info.usedMb;
                    // 计算使用率百分比
                    info.usagePercent = info.totalMb > 0
                        ? static_cast<double>(info.usedMb) / info.totalMb * 100 : 0;
                }
            }
        }
    }
    
    // 关闭管道
    pclose(pipe);
    return info;
}

/**
 * @brief 获取 GPU 当前运行频率
 * 
 * @details 读取 sysfs 文件系统中的 GPU 频率信息。
 *          Jetson 平台的 GPU 频率存储在特定的 sysfs 路径下。
 * 
 * @return double GPU 频率（MHz）
 *         - 成功: 返回当前 GPU 频率（如 921.0 MHz）
 *         - 失败: 返回 0.0
 * 
 * @note sysfs 路径：
 *       "/sys/devices/17000000.gp10b/devfreq/17000000.gp10b/cur_freq"
 *       文件内容为频率值（单位：Hz），需要转换为 MHz
 *       
 * @note 使用场景：
 *       - 监控 GPU 性能状态
 *       - 判断是否处于最大性能模式
 *       - 性能调优和调试
 */
double getGpuFrequency() {
    // 非 Jetson 平台直接返回 0.0
    if (!isJetsonPlatform()) return 0.0;
    
    // GPU 频率文件路径（Jetson Nano 特定路径）
    const char* freqPath = "/sys/devices/17000000.gp10b/devfreq/17000000.gp10b/cur_freq";
    
    std::ifstream f(freqPath);
    if (f.is_open()) {
        std::string line;
        std::getline(f, line);
        
        // 读取频率值（单位：Hz），转换为 MHz
        long freqHz = std::atol(line.c_str());
        return freqHz / 1e6;  // Hz → MHz
    }
    
    return 0.0;
}

/**
 * @brief 根据平台和内存情况优化推理参数
 * 
 * @details 根据是否为 Jetson 平台以及内存使用情况，
 *          自动调整 YOLOv8 推理参数（分辨率、batch_size、FP16）。
 * 
 * @return OptimParams 优化参数结构体
 *         - imgsz: 输入图像分辨率
 *         - batch_size: 批处理大小
 *         - device: 设备ID（0=GPU, -1=CPU）
 *         - fp16: 是否使用半精度推理
 * 
 * @note 参数选择逻辑：
 *       - 非 Jetson 平台: 640x640, batch=8, GPU, 不使用 FP16
 *       - Jetson 平台: 320x320, batch=1, GPU, 使用 FP16
 *       
 * @note 内存警告：
 *       如果内存使用率超过 80%，输出警告信息，
 *       建议降低 batch_size 或分辨率。
 *       
 * @see isJetsonPlatform(), getGpuMemoryInfo()
 */
OptimParams optimizeForJetson() {
    // 非 Jetson 平台：使用高性能参数
    if (!isJetsonPlatform()) {
        return {640, 8, 0, false};  // 640x640, batch=8, GPU, FP32
    }

    // Jetson 平台：获取内存信息
    auto memInfo = getGpuMemoryInfo();
    
    // 检查内存总量（Jetson Nano 通常为 4GB）
    if (memInfo.totalMb > 0 && memInfo.totalMb <= 4096) {
        // 内存使用率超过 80%，输出警告
        if (memInfo.usagePercent > 80) {
            std::cerr << "[Jetson] 内存使用率过高: " << memInfo.usagePercent << "%" << std::endl;
        }
        // 返回优化参数：320x320, batch=1, GPU, FP16
        return {320, 1, 0, true};
    }
    
    // 默认返回 Jetson 优化参数
    return {320, 1, 0, true};
}

/**
 * @brief 开启最大性能模式
 * 
 * @details 使用 jetson_clocks 工具将 Jetson 平台设置为最大性能模式。
 *          该模式会固定 CPU/GPU/EMC 频率到最大值，关闭动态调频。
 * 
 * @return bool 操作是否成功
 *         - true: 成功开启最大性能模式
 *         - false: 操作失败（非 Jetson 平台或权限不足）
 * 
 * @note 命令：sudo jetson_clocks
 *       需要管理员权限（sudo）
 *       
 * @note 效果：
 *       - CPU/GPU/EMC 频率固定到最大值
 *       - 功耗增加，发热量增大
 *       - 推理性能提升约 20-30%
 *       
 * @warning 使用建议：
 *          仅在需要高性能推理时开启，长时间运行可能导致过热。
 *          使用完毕后应调用 disablePerformanceMode() 恢复默认模式。
 *       
 * @see disablePerformanceMode()
 */
bool enablePerformanceMode() {
    if (!isJetsonPlatform()) return false;
    
    // 执行 jetson_clocks 命令（需要 sudo 权限）
    int ret = system("sudo jetson_clocks");
    
    if (ret == 0) {
        std::cout << "[Jetson] 已开启最大性能模式" << std::endl;
        return true;
    }
    
    return false;
}

/**
 * @brief 恢复默认性能模式
 * 
 * @details 使用 jetson_clocks --restore 恢复 Jetson 平台的默认性能设置。
 *          恢复动态调频机制，降低功耗和发热。
 * 
 * @return bool 操作是否成功
 *         - true: 成功恢复默认性能模式
 *         - false: 操作失败（非 Jetson 平台或权限不足）
 * 
 * @note 命令：sudo jetson_clocks --restore
 *       需要管理员权限（sudo）
 *       
 * @note 效果：
 *       - 恢复动态调频机制
 *       - CPU/GPU/EMC 频率根据负载自动调节
 *       - 功耗降低，发热量减少
 *       
 * @see enablePerformanceMode()
 */
bool disablePerformanceMode() {
    if (!isJetsonPlatform()) return false;
    
    // 执行 jetson_clocks --restore 命令（需要 sudo 权限）
    int ret = system("sudo jetson_clocks --restore");
    
    if (ret == 0) {
        std::cout << "[Jetson] 已恢复默认性能模式" << std::endl;
        return true;
    }
    
    return false;
}

/**
 * @brief 打印 Jetson 系统状态信息
 * 
 * @details 输出完整的系统状态报告，包括内存使用、GPU频率、CPU温度等。
 *          格式化的输出便于调试和监控。
 * 
 * @note 输出内容：
 *       - 内存使用：已用、总量、空闲、使用率
 *       - GPU 频率：当前运行频率（MHz）
 *       - CPU 温度：当前 CPU 温度（摄氏度）
 *       
 * @note 温度读取：
 *       CPU 温度从 sysfs 文件读取：
 *       "/sys/devices/virtual/thermal/thermal_zone0/temp"
 *       文件内容为温度值（单位：毫度），需要除以 1000 转换为摄氏度
 *       
 * @see getGpuMemoryInfo(), getGpuFrequency()
 */
void printSystemStatus() {
    // 非 Jetson 平台提示
    if (!isJetsonPlatform()) {
        std::cout << "非 Jetson 平台" << std::endl;
        return;
    }

    // 输出分隔线和标题
    std::cout << "\n" << std::string(50, '=') << "\n";
    std::cout << "Jetson 系统状态\n";
    std::cout << std::string(50, '=') << "\n";

    // 内存使用信息
    auto memInfo = getGpuMemoryInfo();
    std::cout << "\n内存使用:\n";
    std::cout << "  已用: " << memInfo.usedMb << " MB\n";
    std::cout << "  总量: " << memInfo.totalMb << " MB\n";
    std::cout << "  空闲: " << memInfo.freeMb << " MB\n";
    std::cout << "  使用率: " << memInfo.usagePercent << "%\n";

    // GPU 频率
    double gpuFreq = getGpuFrequency();
    if (gpuFreq > 0) {
        std::cout << "\nGPU 频率: " << static_cast<int>(gpuFreq) << " MHz\n";
    }

    // CPU 温度（从 sysfs 读取）
    std::ifstream tempFile("/sys/devices/virtual/thermal/thermal_zone0/temp");
    if (tempFile.is_open()) {
        std::string tempStr;
        std::getline(tempFile, tempStr);
        
        // 温度值单位为毫度，转换为摄氏度
        double temp = std::atof(tempStr.c_str()) / 1000.0;
        std::cout << "CPU 温度: " << temp << "°C\n";
    }

    // 输出分隔线
    std::cout << std::string(50, '=') << "\n";
}

/**
 * @brief 构造函数，初始化性能监控器
 * 
 * @param interval 监控间隔时间（秒），默认值在头文件中定义
 * 
 * @details 初始化监控间隔和运行状态，不立即启动监控线程。
 *          
 * @note 监控间隔建议：
 *       - 快速监控: 1-2 秒（用于调试）
 *       - 正常监控: 5-10 秒（生产环境）
 *       - 低频监控: 30-60 秒（长期运行）
 */
JetsonPerformanceMonitor::JetsonPerformanceMonitor(int interval)
    : m_interval(interval), m_running(false) {}

/**
 * @brief 析构函数，确保监控线程正确停止
 * 
 * @details 自动调用 stop() 方法，确保线程安全退出。
 */
JetsonPerformanceMonitor::~JetsonPerformanceMonitor() {
    stop();  // 确保监控线程停止
}

/**
 * @brief 启动性能监控线程
 * 
 * @details 创建后台线程，按照设定的间隔持续监控系统状态。
 *          仅在 Jetson 平台上有效。
 * 
 * @note 线程安全：
 *       - 使用 std::thread 创建独立线程
 *       - 使用 m_running 标志控制线程退出
 *       - 线程函数为 monitorLoop()
 *       
 * @see monitorLoop(), stop()
 */
void JetsonPerformanceMonitor::start() {
    // 非 Jetson 平台提示
    if (!isJetsonPlatform()) {
        std::cerr << "非 Jetson 平台，监控不可用" << std::endl;
        return;
    }
    
    // 设置运行标志
    m_running = true;
    
    // 创建监控线程
    m_thread = std::thread(&JetsonPerformanceMonitor::monitorLoop, this);
    
    std::cout << "性能监控已启动（间隔 " << m_interval << "s）" << std::endl;
}

/**
 * @brief 停止性能监控线程
 * 
 * @details 设置运行标志为 false，等待线程安全退出。
 *          使用 join() 确保线程完全退出后才返回。
 * 
 * @note 线程退出流程：
 *       1. 设置 m_running = false
 *       2. 线程检测到标志变化，退出循环
 *       3. join() 等待线程函数返回
 */
void JetsonPerformanceMonitor::stop() {
    // 设置停止标志
    m_running = false;
    
    // 等待线程退出
    if (m_thread.joinable()) {
        m_thread.join();
    }
}

/**
 * @brief 监控循环主函数（在独立线程中运行）
 * 
 * @details 持续监控系统内存使用情况，当使用率超过 90% 时输出警告。
 *          使用分段的 sleep 实现可中断的等待，避免线程卡死。
 * 
 * @note 警告阈值：
 *       - 内存使用率 > 90%: 输出警告，建议降低参数
 *       
 * @note 可中断等待：
 *       使用 100ms 的分段 sleep，而不是一次性 sleep m_interval 秒。
 *       这样可以在 stop() 被调用时快速响应（最多延迟 100ms）。
 *       
 * @note 实现细节：
 *       for (int i = 0; i < m_interval * 10 && m_running; ++i)
 *           sleep_for(100ms)
 *       - 每次循环等待 100ms
 *       - 总共循环 m_interval * 10 次，即 m_interval 秒
 *       - 每次 check m_running 标志，实现快速响应退出
 *       
 * @see getGpuMemoryInfo(), start(), stop()
 */
void JetsonPerformanceMonitor::monitorLoop() {
    while (m_running) {
        // 获取内存信息
        auto memInfo = getGpuMemoryInfo();
        
        // 内存使用率超过 90%，输出警告
        if (memInfo.usagePercent > 90) {
            std::cerr << "[警告] 内存使用率过高: " << memInfo.usagePercent << "%" << std::endl;
            std::cerr << "建议: 降低 batch_size 或 imgsz" << std::endl;
        }
        
        // 可中断的等待（分段 sleep）
        // 每次 sleep 100ms，总共 sleep m_interval 秒
        for (int i = 0; i < m_interval * 10 && m_running; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
    }
}
